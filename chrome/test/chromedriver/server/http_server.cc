// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_server.h"

namespace {

// Maximum message size between app and ChromeDriver. Data larger than 150 MB
// or so can cause crashes in Chrome (https://crbug.com/890854), so there is no
// need to support messages that are too large.
const int kBufferSize = 256 * 1024 * 1024;  // 256 MB

int ListenOnIPv4(net::ServerSocket* socket, uint16_t port, bool allow_remote) {
  std::string binding_ip = net::IPAddress::IPv4Localhost().ToString();
  if (allow_remote)
    binding_ip = net::IPAddress::IPv4AllZeros().ToString();
  return socket->ListenWithAddressAndPort(binding_ip, port, 5);
}

int ListenOnIPv6(net::ServerSocket* socket, uint16_t port, bool allow_remote) {
  std::string binding_ip = net::IPAddress::IPv6Localhost().ToString();
  if (allow_remote)
    binding_ip = net::IPAddress::IPv6AllZeros().ToString();
  return socket->ListenWithAddressAndPort(binding_ip, port, 5);
}

bool RequestIsSafeToServe(const net::HttpServerRequestInfo& info,
                          bool allow_remote,
                          const std::vector<net::IPAddress>& whitelisted_ips) {
  // To guard against browser-originating cross-site requests, when host header
  // and/or origin header are present, serve only those coming from localhost
  // or from an explicitly whitelisted ip.
  std::string origin_header = info.GetHeaderValue("origin");
  bool local_origin = false;
  if (!origin_header.empty()) {
    GURL url = GURL(origin_header);
    local_origin = net::IsLocalhost(url);
    if (!local_origin) {
      if (!allow_remote) {
        LOG(ERROR)
            << "Remote connections not allowed; rejecting request with origin: "
            << origin_header;
        return false;
      }
      if (!whitelisted_ips.empty()) {
        net::IPAddress address = net::IPAddress();
        if (!ParseURLHostnameToAddress(origin_header, &address)) {
          LOG(ERROR) << "Unable to parse origin to IPAddress: "
                     << origin_header;
          return false;
        }
        if (!base::Contains(whitelisted_ips, address)) {
          LOG(ERROR) << "Rejecting request with origin: " << origin_header;
          return false;
        }
      }
    }
  }
  // TODO https://crbug.com/chromedriver/3389
  //  When remote access is allowed and origin is not specified,
  // we should confirm that host is current machines ip or hostname

  if (local_origin || !allow_remote) {
    // when origin is localhost host must be localhost
    // when origin is not set, and no remote access, host must be localhost
    std::string host_header = info.GetHeaderValue("host");
    if (!host_header.empty()) {
      GURL url = GURL("http://" + host_header);
      if (!net::IsLocalhost(url)) {
        LOG(ERROR) << "Rejecting request with host: " << host_header
                   << ". origin is " << origin_header;
        return false;
      }
    }
  }
  return true;
}

}  // namespace

HttpServer::HttpServer(const std::string& url_base,
                       const std::vector<net::IPAddress>& whitelisted_ips,
                       const HttpRequestHandlerFunc& handle_request_func,
                       base::WeakPtr<HttpHandler> handler,
                       scoped_refptr<base::SingleThreadTaskRunner> cmd_runner)
    : url_base_(url_base),
      handle_request_func_(handle_request_func),
      allow_remote_(false),
      whitelisted_ips_(whitelisted_ips),
      handler_(handler),
      cmd_runner_(cmd_runner) {}

int HttpServer::Start(uint16_t port, bool allow_remote, bool use_ipv4) {
  allow_remote_ = allow_remote;
  std::unique_ptr<net::ServerSocket> server_socket(
      new net::TCPServerSocket(nullptr, net::NetLogSource()));
  int status = use_ipv4 ? ListenOnIPv4(server_socket.get(), port, allow_remote)
                        : ListenOnIPv6(server_socket.get(), port, allow_remote);
  if (status != net::OK) {
    VLOG(0) << "listen on " << (use_ipv4 ? "IPv4" : "IPv6")
            << " failed with error " << net::ErrorToShortString(status);
    return status;
  }
  server_ = std::make_unique<net::HttpServer>(std::move(server_socket), this);
  net::IPEndPoint address;
  return server_->GetLocalAddress(&address);
}

void HttpServer::OnConnect(int connection_id) {
  server_->SetSendBufferSize(connection_id, kBufferSize);
  server_->SetReceiveBufferSize(connection_id, kBufferSize);
}

void HttpServer::OnHttpRequest(int connection_id,
                               const net::HttpServerRequestInfo& info) {
  if (!RequestIsSafeToServe(info, allow_remote_, whitelisted_ips_)) {
    server_->Send500(connection_id,
                     "Host header or origin header is specified and is not "
                     "whitelisted or localhost.",
                     TRAFFIC_ANNOTATION_FOR_TESTS);
    return;
  }
  handle_request_func_.Run(
      info, base::BindRepeating(&HttpServer::OnResponse,
                                weak_factory_.GetWeakPtr(), connection_id,
                                !info.HasHeaderValue("connection", "close")));
}

HttpServer::~HttpServer() = default;

void HttpServer::OnWebSocketRequest(int connection_id,
                                    const net::HttpServerRequestInfo& info) {
  cmd_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HttpHandler::OnWebSocketRequest, handler_,
                                connection_id, info));

  std::string path = info.path;
  std::string session_id;

  if (!base::StartsWith(path, url_base_, base::CompareCase::SENSITIVE)) {
    net::HttpServerResponseInfo response(net::HTTP_BAD_REQUEST);
    response.SetBody("invalid websocket request url path", "text/plain");
    server_->SendResponse(connection_id, response,
                          TRAFFIC_ANNOTATION_FOR_TESTS);
    return;
  }
  path.erase(0, url_base_.length());

  std::vector<std::string> path_parts =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::string> command_path_parts = base::SplitString(
      kCreateWebSocketPath, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (path_parts.size() != command_path_parts.size()) {
    net::HttpServerResponseInfo response(net::HTTP_BAD_REQUEST);
    response.SetBody("invalid websocket request url path", "text/plain");
    server_->SendResponse(connection_id, response,
                          TRAFFIC_ANNOTATION_FOR_TESTS);
    return;
  }

  for (size_t i = 0; i < path_parts.size(); ++i) {
    if (command_path_parts[i][0] == ':') {
      std::string name = command_path_parts[i];
      name.erase(0, 1);
      CHECK(name.length());
      if (name == "sessionId")
        session_id = path_parts[i];
    } else if (command_path_parts[i] != path_parts[i]) {
      net::HttpServerResponseInfo response(net::HTTP_BAD_REQUEST);
      response.SetBody("invalid websocket request url path", "text/plain");
      server_->SendResponse(connection_id, response,
                            TRAFFIC_ANNOTATION_FOR_TESTS);
      return;
    }
  }

  server_->AcceptWebSocket(connection_id, info, TRAFFIC_ANNOTATION_FOR_TESTS);
  connection_to_session_map[connection_id] = session_id;
}

void HttpServer::OnWebSocketMessage(int connection_id, std::string data) {
  base::Optional<base::Value> parsed_data = base::JSONReader::Read(data);
  std::string path = url_base_ + kSendCommandFromWebSocket;
  base::ReplaceFirstSubstringAfterOffset(
      &path, 0, ":sessionId", connection_to_session_map[connection_id]);

  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = path;
  request.data = data;
  OnHttpRequest(connection_id, request);
}

void HttpServer::OnClose(int connection_id) {
  cmd_runner_->PostTask(FROM_HERE, base::BindOnce(&HttpHandler::OnClose,
                                                  handler_, connection_id));
}

void HttpServer::OnResponse(
    int connection_id,
    bool keep_alive,
    std::unique_ptr<net::HttpServerResponseInfo> response) {
  if (!keep_alive)
    response->AddHeader("Connection", "close");
  server_->SendResponse(connection_id, *response, TRAFFIC_ANNOTATION_FOR_TESTS);
  // Don't need to call server_->Close(), since SendResponse() will handle
  // this for us.
}
