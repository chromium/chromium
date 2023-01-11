// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/ui_devtools/switches.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace ui_devtools {

namespace {
const char kChromeDeveloperToolsPrefix[] =
    "devtools://devtools/bundled/devtools_app.html?uiDevTools=true&ws=";

const base::FilePath::CharType kUIDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("UIDevToolsActivePort");

void WriteUIDevtoolsPortToFile(base::FilePath output_dir, int port) {
  base::FilePath path = output_dir.Append(kUIDevToolsActivePortFileName);
  std::string port_target_string = base::StringPrintf("%d", port);
  if (base::WriteFile(path, port_target_string.c_str(),
                      static_cast<int>(port_target_string.length())) < 0) {
    LOG(ERROR) << "Error writing UIDevTools active port to file";
  }
}
}  // namespace

UiDevToolsServer* UiDevToolsServer::devtools_server_ = nullptr;

const net::NetworkTrafficAnnotationTag UiDevToolsServer::kUIDevtoolsServerTag =
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

UiDevToolsServer::UiDevToolsServer(
    int port,
    net::NetworkTrafficAnnotationTag tag,
    const base::FilePath& active_port_output_directory)
    : port_(port),
      active_port_output_directory_(active_port_output_directory),
      tag_(tag) {
  DCHECK(!devtools_server_);
  devtools_server_ = this;
}

UiDevToolsServer::~UiDevToolsServer() {
  devtools_server_ = nullptr;
}

// static
std::unique_ptr<UiDevToolsServer> UiDevToolsServer::CreateForViews(
    network::mojom::NetworkContext* network_context,
    int port,
    const base::FilePath& active_port_output_directory) {
  // TODO(mhashmi): Change port if more than one inspectable clients
  auto server = base::WrapUnique(new UiDevToolsServer(
      port, kUIDevtoolsServerTag, active_port_output_directory));
  mojo::PendingRemote<network::mojom::TCPServerSocket> server_socket;
  auto receiver = server_socket.InitWithNewPipeAndPassReceiver();
  CreateTCPServerSocket(std::move(receiver), network_context, port,
                        kUIDevtoolsServerTag,
                        base::BindOnce(&UiDevToolsServer::MakeServer,
                                       server->weak_ptr_factory_.GetWeakPtr(),
                                       std::move(server_socket)));
  return server;
}

void UiDevToolsServer::SetOnSocketConnectedForTesting(
    base::OnceClosure on_socket_connected) {
  if (server_) {
    std::move(on_socket_connected).Run();
    return;
  }
  on_socket_connected_ = std::move(on_socket_connected);
}

// static
void UiDevToolsServer::CreateTCPServerSocket(
    mojo::PendingReceiver<network::mojom::TCPServerSocket>
        server_socket_receiver,
    network::mojom::NetworkContext* network_context,
    int port,
    net::NetworkTrafficAnnotationTag tag,
    network::mojom::NetworkContext::CreateTCPServerSocketCallback callback) {
  // Create the socket using the address 127.0.0.1 to listen on all interfaces.
  net::IPAddress address(127, 0, 0, 1);
  constexpr int kBacklog = 1;
  network_context->CreateTCPServerSocket(
      net::IPEndPoint(address, port), kBacklog,
      net::MutableNetworkTrafficAnnotationTag(tag),
      std::move(server_socket_receiver), std::move(callback));
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

// static
bool UiDevToolsServer::IsUiDevToolsEnabled(const char* enable_devtools_flag) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      enable_devtools_flag);
}

// static
int UiDevToolsServer::GetUiDevToolsPort(const char* enable_devtools_flag,
                                        int default_port) {
  // `enable_devtools_flag` is specified only when UiDevTools were started with
  // browser start. If not specified at run time, we should use default port.
  if (!IsUiDevToolsEnabled(enable_devtools_flag))
    return default_port;

  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          enable_devtools_flag);
  int port = 0;
  return base::StringToInt(switch_value, &port) ? port : default_port;
}

void UiDevToolsServer::AttachClient(std::unique_ptr<UiDevToolsClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  clients_.push_back(std::move(client));
}

void UiDevToolsServer::SendOverWebSocket(int connection_id,
                                         base::StringPiece message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  server_->SendOverWebSocket(connection_id, message, tag_);
}

void UiDevToolsServer::SetOnSessionEnded(base::OnceClosure callback) const {
  on_session_ended_ = std::move(callback);
}

void UiDevToolsServer::MakeServer(
    mojo::PendingRemote<network::mojom::TCPServerSocket> server_socket,
    int result,
    const absl::optional<net::IPEndPoint>& local_addr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_sequence_);
  if (result == net::OK) {
    server_ = std::make_unique<network::server::HttpServer>(
        std::move(server_socket), this);
    // When --enable-ui-devtools=0, the browser will pick an available port and
    // write to |kUIDevToolsActivePortFileName|. The file is useful for other
    // programs such as Telemetry to know which port to listen to.
    if (port_ == 0 && local_addr) {
      port_ = local_addr->port();
      if (!active_port_output_directory_.empty()) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock()},
            base::BindOnce(&WriteUIDevtoolsPortToFile,
                           active_port_output_directory_, port_));
      }
    }
  }
  if (on_socket_connected_)
    std::move(on_socket_connected_).Run();
}

// HttpServer::Delegate Implementation
void UiDevToolsServer::OnConnect(int connection_id) {
  base::RecordAction(base::UserMetricsAction("UI_DevTools_Connect"));
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
      target_id >= clients_.size()) {
    return;
  }

  UiDevToolsClient* client = clients_[target_id].get();
  // Only one user can inspect the client at a time
  if (client->connected())
    return;
  client->set_connection_id(connection_id);
  connections_[connection_id] = client;
  server_->AcceptWebSocket(connection_id, info, tag_);
}

void UiDevToolsServer::OnWebSocketMessage(int connection_id, std::string data) {
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

  if (connections_.empty() && on_session_ended_)
    std::move(on_session_ended_).Run();
}

}  // namespace ui_devtools
