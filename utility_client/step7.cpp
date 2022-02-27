/*
 * Websocket Tutorial Step 7.
 * https://docs.websocketpp.org/md_tutorials_utility_client_utility_client.html
 * 
 * Using TLS / Secure WebSockets
 *     * Change the includes
 *     * link to the new library dependencies
 *     * Switch the config
 *     * add the tls_init_handler
 *     * configure the SSL context for desired security level
 *     * mixing secure and non-secure connections in one application (not yet).
 * 
 * The following message is the BSD-3-Clause license which is attatched to the
 * original source code.
 * 
 * /

 /*
  * Copyright (c) 2014, Peter Thorson. All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions are met:
  *     * Redistributions of source code must retain the above copyright
  *       notice, this list of conditions and the following disclaimer.
  *     * Redistributions in binary form must reproduce the above copyright
  *       notice, this list of conditions and the following disclaimer in the
  *       documentation and/or other materials provided with the distribution.
  *     * Neither the name of the WebSocket++ Project nor the
  *       names of its contributors may be used to endorse or promote products
  *       derived from this software without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
  * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */
  
 // **NOTE:** This file is a snapshot of the WebSocket++ utility client tutorial.
 // Additional related material can be found in the tutorials/utility_client
 // directory of the WebSocket++ repository.


//TLS or no TLS
#include <websocketpp/config/asio_client.hpp>	// TLS
//#include <websocketpp/config/asio_no_tls_client.hpp>	// no TLS

#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

//TLS or No TLS
using client = websocketpp::client<websocketpp::config::asio_tls_client>;	//TLS
//using client = websocketpp::client<websocketpp::config::asio_client>;	//no TLS


class connection_metadata {
public:
	using ptr = websocketpp::lib::shared_ptr<connection_metadata>;

	connection_metadata(int id, websocketpp::connection_hdl hdl, std::string uri)
	  : m_id(id)
	   ,m_hdl(hdl)
	   ,m_status("Connecting")
	   ,m_uri(uri)
	   ,m_server("N/A")
	{ }

	void on_open(client *c, websocketpp::connection_hdl hdl)
	{
		m_status = "Open";

		//Get a connection form a handler.
		client::connection_ptr con = c->get_con_from_hdl(hdl);
		m_server = con->get_response_header("Server");
	}

	void on_close(client *c, websocketpp::connection_hdl hdl)
	{
		m_status = "Closed";

		client::connection_ptr con = c->get_con_from_hdl(hdl);
		std::stringstream s;
		s << "close code: " << con->get_remote_close_code() 
		<< "("
		<< websocketpp::close::status::get_string(con->get_remote_close_code())
		<< "), close reason: "
		<< con->get_remote_close_reason();

		m_error_reason = s.str();
	}

	void on_fail(client *c, websocketpp::connection_hdl hdl)
	{
		m_status = "Failed";

		client::connection_ptr con = c->get_con_from_hdl(hdl);
		m_server = con->get_response_header("Server");
		m_error_reason = con->get_ec().message();
	}

	void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg)
	{
		if(msg->get_opcode() == websocketpp::frame::opcode::text)
		{
			m_messages.push_back("<<" + msg->get_payload());
		}
		else
		{
			m_messages.push_back("<<" + websocketpp::utility::to_hex(msg->get_payload()));
		}
	}

	std::string get_status() const
	{
		return m_status;
	}

	int get_id() const
	{
		return m_id;
	}

	websocketpp::connection_hdl get_hdl() const
	{
		return m_hdl;
	}

	void record_sent_message(std::string message)
	{
		m_messages.push_back(">>" + message);
	}

	friend std::ostream & operator<< (std::ostream & out, connection_metadata const & data);

private:

	int m_id;
	websocketpp::connection_hdl m_hdl;
	std::string m_status;
	std::string m_uri;
	std::string m_server;
	std::string m_error_reason;

	std::vector<std::string> m_messages;

};

std::ostream & operator<< (std::ostream & out, connection_metadata const & data)
{
	out 
	<< "> URI: " << data.m_uri << "\n"
	<< "> Status: " << data.m_status << "\n"
	<< "> Remote Server: " << (data.m_server.empty() ? "None Specified" : data.m_server) << "\n"
	<< "> Error/close reason: " << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason) << "\n";

	out
	<< "> Messages Processed: (" << data.m_messages.size() <<")\n";
	std::vector<std::string>::const_iterator it;
	for (it = data.m_messages.begin(); it != data.m_messages.end(); ++it)
	{
		out << *it << "\n";
	}

	return out;
}


class websocket_endpoint
{
public:

	websocket_endpoint()
	{
		m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
		m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

		m_endpoint.init_asio();

		// TLS
		m_endpoint.set_tls_init_handler(websocketpp::lib::bind(&on_tls_init));

		m_endpoint.start_perpetual();

		m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));

	}

	~websocket_endpoint()
	{
		m_endpoint.stop_perpetual();

		for(con_list::const_iterator it = m_connection_list.begin(); 
			it != m_connection_list.end(); 
			++it)
		{
			if (it->second->get_status() != "Open")
			{
				continue;
			}

			std::cout << "> Closing connection " << it->second->get_id() << std::endl;

			websocketpp::lib::error_code ec;
			m_endpoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
			if(ec)
			{
				std::cout 
				<< "> Error closing connection " << it->second->get_id() << ": "
				<< ec.message() << std::endl;
			}
		}

		m_thread->join();
	}
	
	int connect(std::string const & uri)
	{

		websocketpp::lib::error_code ec;
		client::connection_ptr con = m_endpoint.get_connection(uri, ec);

		if(ec)
		{
			std::cout << "> Connect initialization error: " << ec.message() << std::endl;
			return -1;
		}

		int new_id = m_next_id++;
		connection_metadata::ptr metadata_ptr(new connection_metadata(new_id, con->get_handle(), uri));
		m_connection_list[new_id] = metadata_ptr;


        con->set_open_handler(websocketpp::lib::bind(
            &connection_metadata::on_open,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));
        con->set_fail_handler(websocketpp::lib::bind(
            &connection_metadata::on_fail,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));
		con->set_close_handler(websocketpp::lib::bind(
             &connection_metadata::on_close,
             metadata_ptr,
             &m_endpoint,
             websocketpp::lib::placeholders::_1
         ));
        con->set_message_handler(websocketpp::lib::bind(
             &connection_metadata::on_message,
             metadata_ptr,
             websocketpp::lib::placeholders::_1,
             websocketpp::lib::placeholders::_2
         ));

		m_endpoint.connect(con);

		return new_id;

	}

     void close(int id, websocketpp::close::status::value code, std::string reason) {
         websocketpp::lib::error_code ec;
  
         con_list::iterator metadata_it = m_connection_list.find(id);
         if (metadata_it == m_connection_list.end()) {
             std::cout << "> No connection found with id " << id << std::endl;
             return;
         }
  
         m_endpoint.close(metadata_it->second->get_hdl(), code, reason, ec);
         if (ec) {
             std::cout << "> Error initiating close: " << ec.message() << std::endl;
         }
     }

	void send(int id, std::string message)
	{
		websocketpp::lib::error_code ec;

		con_list::iterator metadata_it = m_connection_list.find(id);
		if (metadata_it == m_connection_list.end())
		{
			std::cout << "> No connection found with id " << id << std::endl;
			return;
		}

		m_endpoint.send(metadata_it->second->get_hdl(), message, websocketpp::frame::opcode::text, ec);
		if(ec){
			std::cout << "> Error sending message: " << ec.message() << std::endl;
			return;
		}

		metadata_it->second->record_sent_message(message);
	}

	connection_metadata::ptr get_metadata(int id) const
	{
		con_list::const_iterator metadata_it = m_connection_list.find(id);
		if( metadata_it == m_connection_list.end())
		{
			return connection_metadata::ptr();
		} else {
			return metadata_it->second;
		}
	}

	using context_ptr = std::shared_ptr<boost::asio::ssl::context>;

	static context_ptr on_tls_init()
	{
		context_ptr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

		try {
			ctx->set_options(
				boost::asio::ssl::context::default_workarounds
				| boost::asio::ssl::context::no_sslv2
				| boost::asio::ssl::context::no_sslv3
				| boost::asio::ssl::context::single_dh_use
			);

		} catch (std::exception &e)
		{
			std::cout << "Error in context pointer: " << e.what() << std::endl;
		}
		return ctx;
	}

private:
	using con_list = std::map<int, connection_metadata::ptr>;

	client m_endpoint;
	websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

	con_list m_connection_list;
	int m_next_id;
};

int main()
{
	bool done = false;
	std::string input;
	websocket_endpoint endpoint;

	while(!done)
	{
		std::cout << "Enter Command:";
		std::getline(std::cin, input);

		if (input == "quit")
		{
			done = true;
		}
		else if (input == "help")
		{
			std::cout
				<< "\n"
				<< "Command List:\n"
				<< "connect <ws uri>\n"
				<< "show <coonection_id>\n"
				<< "send\n"
				<< "close <connection_id> (<close code> <close reason>)\n"
				<< "help: Display this help text\n"
				<< "quit: Exit the program\n"
				<< std::endl;
		} else if(input.substr(0, 7) == "connect")
		{
			int id{};
			if(input.size() > 7)
			{
				id = endpoint.connect(input.substr(8));
			} else {
				id = endpoint.connect("wss://ws.vi-server.org/mirror");
			}
			if( id != -1)
			{
				std::cout << "> Created connection with id " << id << std::endl;
			}

		} else if (input.substr(0, 4) == "show")
		{
			int id = atoi(input.substr(5).c_str());

			connection_metadata::ptr metadata = endpoint.get_metadata(id);
			if(metadata)
			{
				std::cout << *metadata << std::endl;
			} else 
			{
				std::cout << "> Unknown connection id " << id << std::endl;
			}
		}
		else if (input.substr(0,4) == "send")
		{
			std::stringstream ss(input);
			std::string cmd;
			int id;
			std::string message(input.substr(5));

			ss >> cmd >> id;
			std::getline(ss, message);
			endpoint.send(id, message);
		}
		else if (input.substr(0, 5) == "close")
		{
			std::stringstream ss(input);

			std::string cmd;
			int id;
			int close_code = websocketpp::close::status::normal;
			std::string reason;
			ss >> cmd >> id >> close_code;
			std::getline(ss, reason);
			endpoint.close(id, close_code, reason);
		}
		else
		{
			std::cout << "Unrecognized Command" << std::endl;
		}
	}

	return 0;
}

/*
 * Ubuntu-20.04 (WSL)
 * 
 * sudo apt install g++
 * sudo apt install libboost-dev
 * sudo apt install libboost-thread-dev
 * sudo apt install libwebsocketpp-dev
 * 
 * g++ -std=c++2a step7.cpp -o step7 -lpthread -lboost_system -lboost_thread -lssl -lcrypto -ldl
 * 
*/