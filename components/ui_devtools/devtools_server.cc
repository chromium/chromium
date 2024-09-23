// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "components/ui_devtools/switches.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ui_devtools {

namespace {
const char kChromeDeveloperToolsPrefix[] =
    "devtools://devtools/bundled/devtools_app.html?uiDevTools=true&ws=";

const base::FilePath::CharType kUIDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("UIDevToolsActivePort");

void WriteUIDevtoolsPortToFile(base::FilePath output_dir, int port) {
  base::FilePath path = output_dir.Append(kUIDevToolsActivePortFileName);
  std::string port_target_string = base::StringPrintf("%d", port);
  if (!base::WriteFile(path, port_target_string)) {
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

class UiDevToolsServer::IOThreadData : public net::HttpServer::Delegate {
 public:
  IOThreadData(base::WeakPtr<UiDevToolsServer> owner,
               net::NetworkTrafficAnnotationTag tag);
  IOThreadData(IOThreadData&) = delete;
  ~IOThreadData() override = default;

  IOThreadData& operator=(IOThreadData&) = delete;

  // Creates a ServerSocket and an HttpServer and starts listening for
  // connections.
  void MakeServer(int port, base::FilePath active_port_output_directory);

  // These wrap the corresponding HttpServer functions.
  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);
  void SendOverWebSocket(int connection_id, std::string data);

 private:
  // HttpServer::Delegate:
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

  base::WeakPtr<UiDevToolsServer> owner_;
  const scoped_refptr<base::SequencedTaskRunner> main_sequence_task_runner_;
  std::unique_ptr<net::HttpServer> server_
      GUARDED_BY_CONTEXT(io_thread_sequence_);

  const net::NetworkTrafficAnnotationTag tag_;

  SEQUENCE_CHECKER(io_thread_sequence_);
};

UiDevToolsServer::IOThreadData::IOThreadData(
    base::WeakPtr<UiDevToolsServer> owner,
    net::NetworkTrafficAnnotationTag tag)
    : owner_(owner),
      main_sequence_task_runner_(
          base::SequencedTaskRunner::GetCurrentDefault()),
      tag_(tag) {
  DETACH_FROM_SEQUENCE(io_thread_sequence_);
}

void UiDevToolsServer::IOThreadData::MakeServer(
    int port,
    base::FilePath active_port_output_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  DCHECK(!server_);

  // Create the socket using the address 127.0.0.1 to listen on all interfaces.
  constexpr int kBacklog = 1;
  std::unique_ptr<net::ServerSocket> socket =
      std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
  if (socket->Listen(net::IPEndPoint(net::IPAddress::IPv4Localhost(), port),
                     kBacklog, /*ipv6_only=*/std::nullopt) == net::OK) {
    server_ = std::make_unique<net::HttpServer>(std::move(socket), this);
    // When --enable-ui-devtools=0, the browser will pick an available port and
    // write to |kUIDevToolsActivePortFileName|. The file is useful for other
    // programs such as Telemetry to know which port to listen to.
    net::IPEndPoint local_addr;
    if (port == 0 && server_->GetLocalAddress(&local_addr) == net::OK) {
      port = local_addr.port();
      if (!active_port_output_directory.empty()) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock()},
            base::BindOnce(&WriteUIDevtoolsPortToFile,
                           std::move(active_port_output_directory), port));
      }
    }
  }

  main_sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UiDevToolsServer::ServerCreated, owner_, port));
}

void UiDevToolsServer::IOThreadData::OnConnect(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  base::RecordAction(base::UserMetricsAction("UI_DevTools_Connect"));
}

void UiDevToolsServer::IOThreadData::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  NOTIMPLEMENTED();
}

void UiDevToolsServer::IOThreadData::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  main_sequence_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UiDevToolsServer::OnWebSocketRequest, owner_,
                                connection_id, info));
}

void UiDevToolsServer::IOThreadData::OnWebSocketMessage(int connection_id,
                                                        std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  main_sequence_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UiDevToolsServer::OnWebSocketMessage, owner_,
                                connection_id, std::move(data)));
}

void UiDevToolsServer::IOThreadData::OnClose(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  main_sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UiDevToolsServer::OnClose, owner_, connection_id));
}

void UiDevToolsServer::ServerCreated(int port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  connected_ = true;
  port_ = port;
  if (on_socket_connected_) {
    std::move(on_socket_connected_).Run();
  }
}

void UiDevToolsServer::IOThreadData::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  server_->AcceptWebSocket(connection_id, request, tag_);
}

void UiDevToolsServer::IOThreadData::SendOverWebSocket(int connection_id,
                                                       std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_thread_sequence_);
  server_->SendOverWebSocket(connection_id, data, tag_);
}

UiDevToolsServer::UiDevToolsServer(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    int port,
    net::NetworkTrafficAnnotationTag tag,
    const base::FilePath& active_port_output_directory)
    : io_thread_task_runner_(std::move(io_thread_task_runner)), port_(port) {
  DCHECK(!devtools_server_);
  devtools_server_ = this;

  // Can't initialize this in the initializer list, since it depends on
  // `weak_ptr_factory_`, which tooling requires be declared last in the class,
  // and thus is the last field to be initialized.
  io_thread_data_ =
      std::make_unique<IOThreadData>(weak_ptr_factory_.GetWeakPtr(), tag);

  io_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UiDevToolsServer::IOThreadData::MakeServer,
                                base::Unretained(io_thread_data_.get()), port,
                                active_port_output_directory));
}

UiDevToolsServer::~UiDevToolsServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  io_thread_task_runner_->DeleteSoon(FROM_HERE, std::move(io_thread_data_));
  devtools_server_ = nullptr;
}

// static
std::unique_ptr<UiDevToolsServer> UiDevToolsServer::CreateForViews(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    int port,
    const base::FilePath& active_port_output_directory) {
  // TODO(mhashmi): Change port if more than one inspectable clients
  return base::WrapUnique(new UiDevToolsServer(std::move(io_thread_task_runner),
                                               port, kUIDevtoolsServerTag,
                                               active_port_output_directory));
}

void UiDevToolsServer::SetOnSocketConnectedForTesting(
    base::OnceClosure on_socket_connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  if (connected_) {
    std::move(on_socket_connected).Run();
    return;
  }
  on_socket_connected_ = std::move(on_socket_connected);
}

void UiDevToolsServer::OnWebSocketRequestForTesting(
    int connection_id,
    net::HttpServerRequestInfo info) {
  OnWebSocketRequest(connection_id, std::move(info));
}

// static
std::vector<UiDevToolsServer::NameUrlPair>
UiDevToolsServer::GetClientNamesAndUrls() {
  std::vector<NameUrlPair> pairs;
  if (!devtools_server_)
    return pairs;

  DCHECK_CALLED_ON_VALID_SEQUENCE(devtools_server_->main_sequence_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  clients_.push_back(std::move(client));
}

void UiDevToolsServer::SendOverWebSocket(int connection_id,
                                         std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IOThreadData::SendOverWebSocket,
                                base::Unretained(io_thread_data_.get()),
                                connection_id, std::string(message)));
}

void UiDevToolsServer::SetOnSessionEnded(base::OnceClosure callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  on_session_ended_ = std::move(callback);
}

void UiDevToolsServer::OnWebSocketRequest(int connection_id,
                                          net::HttpServerRequestInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
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
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IOThreadData::AcceptWebSocket,
                                base::Unretained(io_thread_data_.get()),
                                connection_id, std::move(info)));
}

void UiDevToolsServer::OnWebSocketMessage(int connection_id, std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  auto it = connections_.find(connection_id);
  CHECK(it != connections_.end(), base::NotFatalUntil::M130);
  UiDevToolsClient* client = it->second;
  client->Dispatch(data);
}

void UiDevToolsServer::OnClose(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  auto it = connections_.find(connection_id);
  if (it == connections_.end())
    return;
  UiDevToolsClient* client = it->second;
  client->Disconnect();
  connections_.erase(it);

  if (connections_.empty() && on_session_ended_) {
    // `on_session_ended_` may destroy this, but there's nothing else on the
    // call stack, so this should be safe.
    std::move(on_session_ended_).Run();
  }

  // `this` may have been destroyed at this point.
}

}  // namespace ui_devtools
