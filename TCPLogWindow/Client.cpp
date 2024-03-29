#include "Client.h"

#include <windows.h>
#include <iostream>
#include <exception>

#define TEST_SERVER_ADDRESS 127, 0, 0, 1, 55555
#define DATA_BUFFER_SIZE 1024 * 1024

Client::Client()
	: mDataBuffer(DATA_BUFFER_SIZE)
{
	std::string body("Program init OK. ");
	std::unique_ptr<Request> initialRequest(std::make_unique<Request>(
	"/program",
	Level::Info,
	body.length(),
	body));
	mRequestQueue.push_back(std::move(initialRequest));
}

void Client::run()
{
	printf("Client: \n");

	// handle user input ip and port
	std::string addrString;
	ip_address addr;
	std::cout << "Enter server IP address and port" << std::endl;
	std::cout << "Example: 127.0.0.1:55555" << std::endl;
	std::cout << "IP address and port: ";
	std::cin >> addrString;
	while (!stringToAddr(addrString, addr))
	{
		std::cout << "Wrong format, enter again: ";
		std::cin >> addrString;
	}

	if (!connect(addr))
		return;

	while (true)
	{
		if (!send())
			return;

		mRequestQueue.clear();

		// receive
		size_t receivedLength;
		if (receive(mDataBuffer.getData(), mDataBuffer.getSize(), receivedLength))
		{
			if (receivedLength > 0)
			{
				printf("Server response: \n%s\n", mDataBuffer.getData());
				mDataBuffer.clear();
			}
		}
		else
		{
			return;
		}

		listenUserRequest();
	}
}

void Client::listenUserRequest()
{
	std::string requestBody;
	std::cout << "Request: ";
	std::cin >> requestBody;

	std::unique_ptr<Request> userRequest(std::make_unique<Request>(
		"/user",
		Level::Debug,
		requestBody.length(),
		requestBody));
	mRequestQueue.push_back(std::move(userRequest));
}

bool Client::send()
{
	for (auto it = mRequestQueue.begin(); it != mRequestQueue.end(); ++it)
	{
		if (!sendRequest(it->get()))
			return false;
	}
	return true;
}

bool Client::sendRequest(Request* request)
{
	std::string requestString = request->formRequestStringClient();
	printf("Sending request...\n#request begin\n%s\n#end of request\n\n", requestString.c_str());
	size_t length = requestString.length();
	size_t sent = 0;
	while (sent < length)
	{
		auto result = mSocket.send((uint32_t)(requestString.length() - sent), (uint8*)requestString.c_str() + sent);
		if (result.is_success())
		{
			sent += result.length_;
		}
		else
		{
			auto errcode = result.code_;
			auto errmsg = network::error::as_string(errcode);
			printf("failed to send - %s (%d) \n", errmsg, errcode);
			return false;
		}
	}

	return true;
}

bool Client::connect(const ip_address& serverAddr)
{
	auto result = mSocket.connect(serverAddr);
	if (result.is_failure())
	{
		auto errcode = result.code_;
		auto errmsg = network::error::as_string(errcode);
		printf("failed to connect to server - %s (%d) \n", errmsg, errcode);
		return false;
	}
	return true;
}

bool Client::receive(char* dataBuffer, size_t bufferSize, size_t& receivedLength)
{
	receivedLength = 0;

	while (receivedLength < bufferSize)
	{
		auto receiveResult = mSocket.receive(
			(uint32_t)(bufferSize - receivedLength),
			(uint8*)dataBuffer);

		receivedLength += receiveResult.length_;

		if (receiveResult.length_ == 0)
		{
			if (!network::error::is_non_critical(receiveResult.code_))
			{
				auto errcode = network::error::get_error();
				auto errmsg = network::error::as_string(errcode);
				printf("cannot receive from server - %s (%d) \n", errmsg, errcode);
				return false;
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

bool Client::stringToAddr(std::string& addrString, ip_address& addr)
{
	std::string tokens[5];
	const std::string dot(".");
	const std::string colum(":");
	size_t pos = 0;
	size_t ipTokenCount = 0;

	std::string ipString;

	// split the ip and port
	if ((pos = addrString.find(colum)) != std::string::npos)
	{
		ipString = addrString.substr(0, pos);
		addrString.erase(0, pos + colum.length());
		tokens[4] = std::move(addrString);
	}

	while ((pos = ipString.find(dot)) != std::string::npos)
	{
		std::string ipToken = ipString.substr(0, pos);
		tokens[ipTokenCount] = std::move(ipToken);
		++ipTokenCount;
		ipString.erase(0, pos + dot.length());
	}

	if (ipTokenCount != 3)
		return false;

	tokens[ipTokenCount] = std::move(ipString);


	try
	{
		addr = ip_address(std::stoi(tokens[0]),
						  std::stoi(tokens[1]),
						  std::stoi(tokens[2]),
						  std::stoi(tokens[3]),
						  std::stoi(tokens[4]));
	}
	catch(std::exception e)
	{
		return false;
	}

	return true;
}
