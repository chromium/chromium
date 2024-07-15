// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_server.h"

#include "base/task/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/sys_addrinfo.h"
#include "url/gurl.h"

namespace {

// Maximum message size between app and ChromeDriver. Data larger than 150 MB
// or so can cause crashes in Chrome (https://crbug.com/890854), so there is no
// need to support messages that are too large.
const int kBufferSize = 256 * 1024 * 1024;  // 256 MB
const char kAnyHostPattern[] = "*";

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

// Heuristic to check if hostname is fully qualified
bool IsSimple(const std::string& hostname) {
  return hostname.find('.') == std::string::npos;
}

bool IsMatch(const std::string& system_host, const std::string& hostname) {
  return hostname == system_host ||
         (base::StartsWith(system_host, hostname) && IsSimple(hostname) &&
          system_host[hostname.size()] == '.');
}

void GetCanonicalHostName(std::vector<std::string>* canonical_host_names) {
  struct addrinfo hints, *info = nullptr, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  auto hostname = net::GetHostName();
  int gai_result;
  if ((gai_result = getaddrinfo(hostname.c_str(), "http", &hints, &info)) !=
      0) {
    LOG(ERROR) << "GetCanonicalHostName Error hostname: " << hostname;
  }
  for (p = info; p != nullptr; p = p->ai_next) {
    if (p->ai_canonname != hostname)
      canonical_host_names->emplace_back(p->ai_canonname);
  }

  if (canonical_host_names->empty())
    canonical_host_names->emplace_back(hostname);

  freeaddrinfo(info);
  return;
}

bool HostIsSafeToServe(GURL host_url,
                       std::string host_header_value,
                       const std::vector<net::IPAddress>& whitelisted_ips,
                       const std::vector<std::string>& allowed_origins) {
  auto host = host_url.host();
  // Check if the origin is in the allowed origins.
  for (const std::string& allowed_origin : allowed_origins) {
    if (allowed_origin == kAnyHostPattern) {
      // Allow any host origin in case of `allowed-origins` contains `*`.
      return true;
    }
    if (allowed_origin == host) {
      // Allow host from `allowed-origins`.
      return true;
    }
  }

  net::IPAddress host_address = net::IPAddress();
  if (ParseURLHostnameToAddress(host, &host_address)) {
    net::NetworkInterfaceList list;
    if (net::GetNetworkList(&list,
                            net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
      for (const auto& network_interface : list) {
        if (network_interface.address == host_address) {
          return true;
        }
      }

      LOG(ERROR) << "Rejecting request with host: " << host_header_value
                 << " address: " << host_address.ToString();
      return false;
    }
    return true;
  }

  static std::vector<std::string> canonical_host_names;
  GetCanonicalHostName(&canonical_host_names);
  for (const auto& system_host : canonical_host_names) {
    if (IsMatch(system_host, host)) {
      return true;
    }
  }

  LOG(ERROR) << "Unable find match for host: " << host_header_value;
  return false;
}

bool RequestIsSafeToServe(const net::HttpServerRequestInfo& info,
                          bool allow_remote,
                          const std::vector<net::IPAddress>& whitelisted_ips,
                          const std::vector<std::string>& allowed_origins) {
  std::string origin_header_value = info.GetHeaderValue("origin");
  std::string host_header_value = info.GetHeaderValue("host");
  bool is_origin_set = !origin_header_value.empty();
  GURL origin_url(origin_header_value);
  bool is_origin_local = is_origin_set && net::IsLocalhost(origin_url);
  bool is_host_set = !host_header_value.empty();
  GURL host_url("http://" + host_header_value);
  bool is_host_local = is_host_set && net::IsLocalhost(host_url);

  // If origin is localhost, then host needs to be localhost as well.
  if (is_origin_local && !is_host_local) {
    LOG(ERROR) << "Rejecting request with localhost origin but host: "
               << host_header_value;
    return false;
  }

  if (!allow_remote) {
    // If remote is not allowed, both origin and host header need to be
    // localhost or not specified.
    if (is_origin_set && !is_origin_local) {
      LOG(ERROR) << "Rejecting request with non-local origin: "
                 << origin_header_value;
      return false;
    }

    if (is_host_set && !is_host_local) {
      LOG(ERROR) << "Rejecting request with non-local host: "
                 << host_header_value;
      return false;
    }
  } else {
    if (is_origin_set && !is_origin_local) {
      // Check against allowed list where empty allowed list is special case to
      // allow all. Disallow any other non-local origin.
      bool allow_all = whitelisted_ips.empty();
      if (!allow_all) {
        LOG(ERROR) << "Rejecting request with origin set: "
                   << origin_header_value;
        return false;
      }
    }

    if (is_host_set && !is_host_local) {
      return HostIsSafeToServe(host_url, host_header_value, whitelisted_ips,
                               allowed_origins);
    }
  }

  return true;
}

}  // namespace

HttpServer::HttpServer(const std::string& url_base,
                       const std::vector<net::IPAddress>& whitelisted_ips,
                       const std::vector<std::string>& allowed_origins,
                       const HttpRequestHandlerFunc& handle_request_func,
                       base::WeakPtr<HttpHandler> handler,
                       scoped_refptr<base::SingleThreadTaskRunner> cmd_runner)
    : url_base_(url_base),
      handle_request_func_(handle_request_func),
      allow_remote_(false),
      whitelisted_ips_(whitelisted_ips),
      allowed_origins_(allowed_origins),
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
  return server_->GetLocalAddress(&local_address_);
}

const net::IPEndPoint& HttpServer::LocalAddress() const {
  return local_address_;
}

void HttpServer::OnConnect(int connection_id) {
  server_->SetSendBufferSize(connection_id, kBufferSize);
  server_->SetReceiveBufferSize(connection_id, kBufferSize);
}

void HttpServer::OnHttpRequest(int connection_id,
                               const net::HttpServerRequestInfo& info) {
  if (!RequestIsSafeToServe(info, allow_remote_, whitelisted_ips_,
                            allowed_origins_)) {
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
                                this, connection_id, info));
}

void HttpServer::OnWebSocketMessage(int connection_id, std::string data) {
  cmd_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HttpHandler::OnWebSocketMessage, handler_,
                                this, connection_id, data));
}

void HttpServer::OnClose(int connection_id) {
  cmd_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&HttpHandler::OnClose, handler_, this, connection_id));
}

void HttpServer::Close(int connection_id) {
  server_->Close(connection_id);
}

void HttpServer::SendOverWebSocket(int connection_id, const std::string& data) {
  server_->SendOverWebSocket(connection_id, data, TRAFFIC_ANNOTATION_FOR_TESTS);
}

void HttpServer::AcceptWebSocket(int connection_id,
                                 const net::HttpServerRequestInfo& request) {
  server_->AcceptWebSocket(connection_id, request,
                           TRAFFIC_ANNOTATION_FOR_TESTS);
}

void HttpServer::SendResponse(
    int connection_id,
    const net::HttpServerResponseInfo& response,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  server_->SendResponse(connection_id, response, traffic_annotation);
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
