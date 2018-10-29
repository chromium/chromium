// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace ui_devtools {

namespace {
const char kChromeDeveloperToolsPrefix[] =
    "chrome-devtools://devtools/bundled/devtools_app.html?ws=";

bool IsDevToolsEnabled(const char* enable_devtools_flag) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      enable_devtools_flag);
}

int GetUiDevToolsPort(const char* enable_devtools_flag, int default_port) {
  DCHECK(IsDevToolsEnabled(enable_devtools_flag));
  // This value is duplicated in the chrome://flags description.
  int port;
  if (!base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              enable_devtools_flag),
          &port))
    port = default_port;
  return port;
}

constexpr net::NetworkTrafficAnnotationTag kUIDevtoolsServer =
    net::DefineNetworkTrafficAnnotation("ui_devtools_server", R"(
      semantics {
        sender: "UI Devtools Server"
        description:
          "Backend for UI DevTools, to inspect Aura/Views UI."
        trigger:
          "Run with '--enable-ui-devtools' switch."
        data: "Debugging data, including any data on the open pages."
        destination: OTHER
        destination_other: "The data can be sent to any destination."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with '--enable-ui-devtools' switch."
        policy_exception_justification:
          "Not implemented, only used in Devtools and is behind a switch."
      })");

}  // namespace

UiDevToolsServer* UiDevToolsServer::devtools_server_ = nullptr;

UiDevToolsServer::UiDevToolsServer(const char* enable_devtools_flag,
                                   int default_port)
    : port_(GetUiDevToolsPort(enable_devtools_flag, default_port)),
      weak_ptr_factory_(this) {
  DCHECK(!devtools_server_);
  devtools_server_ = this;
}

UiDevToolsServer::~UiDevToolsServer() {
  devtools_server_ = nullptr;
}

// static
std::unique_ptr<UiDevToolsServer> UiDevToolsServer::Create(
    network::mojom::NetworkContext* network_context,
    const char* enable_devtools_flag,
    int default_port) {
  std::unique_ptr<UiDevToolsServer> server;
  if (IsDevToolsEnabled(enable_devtools_flag) && !devtools_server_) {
    // TODO(mhashmi): Change port if more than one inspectable clients
    server.reset(new UiDevToolsServer(enable_devtools_flag, default_port));
    server->Start(network_context, "0.0.0.0");
  }
  return server;
}

// static
std::vector<UiDevToolsServer::NameUrlPair>
UiDevToolsServer::GetClientNamesAndUrls() {
  std::vector<NameUrlPair> pairs;
  if (!devtools_server_)
    return pairs;

  for (ClientsList::size_type i = 0; i != devtools_server_->clients_.size();
       i++) {
    pairs.push_back(std::pair<std::string, std::string>(
        devtools_server_->clients_[i]->name(),
        base::StringPrintf("%s127.0.0.1:%d/%" PRIuS,
                           kChromeDeveloperToolsPrefix,
                           devtools_server_->port(), i)));
  }
  return pairs;
}

void UiDevToolsServer::AttachClient(std::unique_ptr<UiDevToolsClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  clients_.push_back(std::move(client));
}

void UiDevToolsServer::SendOverWebSocket(int connection_id,
                                         const String& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  server_->SendOverWebSocket(connection_id, message, kUIDevtoolsServer);
}

void UiDevToolsServer::Start(network::mojom::NetworkContext* network_context,
                             const std::string& address_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  DCHECK(!server_);

  network::mojom::TCPServerSocketPtr server_socket;
  net::IPAddress address;

  if (!address.AssignFromIPLiteral(address_string))
    return;

  constexpr int kBacklog = 1;
  auto request = mojo::MakeRequest(&server_socket);
  network_context->CreateTCPServerSocket(
      net::IPEndPoint(address, port_), kBacklog,
      net::MutableNetworkTrafficAnnotationTag(kUIDevtoolsServer),
      std::move(request),
      base::BindOnce(&UiDevToolsServer::MakeServer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(server_socket)));
}

void UiDevToolsServer::MakeServer(
    network::mojom::TCPServerSocketPtr server_socket,
    int result,
    const base::Optional<net::IPEndPoint>& local_addr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  if (result == net::OK) {
    server_ = std::make_unique<network::server::HttpServer>(
        std::move(server_socket), this);
  }
}

// HttpServer::Delegate Implementation
void UiDevToolsServer::OnConnect(int connection_id) {
  NOTIMPLEMENTED();
}

void UiDevToolsServer::OnHttpRequest(
    int connection_id,
    const network::server::HttpServerRequestInfo& info) {
  NOTIMPLEMENTED();
}

void UiDevToolsServer::OnWebSocketRequest(
    int connection_id,
    const network::server::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  size_t target_id = 0;
  if (info.path.empty() ||
      !base::StringToSizeT(info.path.substr(1), &target_id) ||
      target_id > clients_.size())
    return;

  UiDevToolsClient* client = clients_[target_id].get();
  // Only one user can inspect the client at a time
  if (client->connected())
    return;
  client->set_connection_id(connection_id);
  connections_[connection_id] = client;
  server_->AcceptWebSocket(connection_id, info, kUIDevtoolsServer);
}

void UiDevToolsServer::OnWebSocketMessage(int connection_id,
                                          const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  auto it = connections_.find(connection_id);
  DCHECK(it != connections_.end());
  UiDevToolsClient* client = it->second;
  client->Dispatch(data);
}

void UiDevToolsServer::OnClose(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  auto it = connections_.find(connection_id);
  if (it == connections_.end())
    return;
  UiDevToolsClient* client = it->second;
  client->Disconnect();
  connections_.erase(it);
}

}  // namespace ui_devtools
