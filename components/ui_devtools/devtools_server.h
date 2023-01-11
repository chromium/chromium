// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_
#define COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/ui_devtools/devtools_client.h"
#include "components/ui_devtools/devtools_export.h"
#include "components/ui_devtools/dom.h"
#include "components/ui_devtools/forward.h"
#include "components/ui_devtools/protocol.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/server/http_server.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ui_devtools {

class TracingAgent;

class UI_DEVTOOLS_EXPORT UiDevToolsServer
    : public network::server::HttpServer::Delegate {
 public:
  // Network tags to be used for the UI devtools servers.
  static const net::NetworkTrafficAnnotationTag kUIDevtoolsServerTag;

  UiDevToolsServer(const UiDevToolsServer&) = delete;
  UiDevToolsServer& operator=(const UiDevToolsServer&) = delete;

  ~UiDevToolsServer() override;

  // Returns an empty unique_ptr if ui devtools flag isn't enabled or if a
  // server instance has already been created. If |port| is 0, the server will
  // choose an available port. If |port| is 0 and |active_port_output_directory|
  // is present, the server will write the chosen port to
  // |kUIDevToolsActivePortFileName| on |active_port_output_directory|.
  static std::unique_ptr<UiDevToolsServer> CreateForViews(
      network::mojom::NetworkContext* network_context,
      int port,
      const base::FilePath& active_port_output_directory = base::FilePath());

  // Creates a TCPServerSocket to be used by a UiDevToolsServer.
  static void CreateTCPServerSocket(
      mojo::PendingReceiver<network::mojom::TCPServerSocket>
          server_socket_receiver,
      network::mojom::NetworkContext* network_context,
      int port,
      net::NetworkTrafficAnnotationTag tag,
      network::mojom::NetworkContext::CreateTCPServerSocketCallback callback);

  // Returns a list of attached UiDevToolsClient name + URL
  using NameUrlPair = std::pair<std::string, std::string>;
  static std::vector<NameUrlPair> GetClientNamesAndUrls();

  // Returns true if UI Devtools is enabled by the given commandline switch.
  static bool IsUiDevToolsEnabled(const char* enable_devtools_flag);

  // Returns the port number specified by a command line flag. If a number is
  // not specified as a command line argument, returns the |default_port|.
  static int GetUiDevToolsPort(const char* enable_devtools_flag,
                               int default_port);

  void AttachClient(std::unique_ptr<UiDevToolsClient> client);
  void SendOverWebSocket(int connection_id, base::StringPiece message);

  int port() const { return port_; }

  TracingAgent* tracing_agent() { return tracing_agent_; }
  void set_tracing_agent(TracingAgent* agent) { tracing_agent_ = agent; }

  // Sets the callback which will be invoked when the session is closed.
  // Marks as a const function so it can be called after the server is set up
  // and used as a constant instance.
  void SetOnSessionEnded(base::OnceClosure callback) const;
  // Sets a callback that tests can use to wait for the server to be ready to
  // accept connections.
  void SetOnSocketConnectedForTesting(base::OnceClosure on_socket_connected);

 private:
  UiDevToolsServer(int port,
                   const net::NetworkTrafficAnnotationTag tag,
                   const base::FilePath& active_port_output_directory);

  void MakeServer(
      mojo::PendingRemote<network::mojom::TCPServerSocket> server_socket,
      int result,
      const absl::optional<net::IPEndPoint>& local_addr);

  // HttpServer::Delegate
  void OnConnect(int connection_id) override;
  void OnHttpRequest(
      int connection_id,
      const network::server::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(
      int connection_id,
      const network::server::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

  using ClientsList = std::vector<std::unique_ptr<UiDevToolsClient>>;
  using ConnectionsMap = std::map<uint32_t, UiDevToolsClient*>;
  ClientsList clients_;
  ConnectionsMap connections_;

  std::unique_ptr<network::server::HttpServer> server_;

  // The port the devtools server listens on
  int port_;

  // Output directory for |kUIDevToolsActivePortFileName| when
  // --enable-ui-devtools=0.
  base::FilePath active_port_output_directory_;

  const net::NetworkTrafficAnnotationTag tag_;

  TracingAgent* tracing_agent_ = nullptr;

  // Invoked when the server doesn't have any live connection.
  mutable base::OnceClosure on_session_ended_;
  base::OnceClosure on_socket_connected_;

  // The server (owned by Chrome for now)
  static UiDevToolsServer* devtools_server_;

  SEQUENCE_CHECKER(devtools_server_sequence_);
  base::WeakPtrFactory<UiDevToolsServer> weak_ptr_factory_{this};
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_
