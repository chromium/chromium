// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_
#define COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_

#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "components/ui_devtools/devtools_client.h"
#include "components/ui_devtools/devtools_export.h"
#include "components/ui_devtools/dom.h"
#include "components/ui_devtools/forward.h"
#include "components/ui_devtools/protocol.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/server/http_server_request_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ui_devtools {

class TracingAgent;

class UI_DEVTOOLS_EXPORT UiDevToolsServer {
 public:
  // Network tags to be used for the UI devtools servers.
  static const net::NetworkTrafficAnnotationTag kUIDevtoolsServerTag;

  UiDevToolsServer(const UiDevToolsServer&) = delete;
  UiDevToolsServer& operator=(const UiDevToolsServer&) = delete;

  ~UiDevToolsServer();

  // Returns an empty unique_ptr if ui devtools flag isn't enabled or if a
  // server instance has already been created. All network activity is performed
  // on `io_thread_task_runner`, which must run tasks on a thread with an IO
  // message pump. All other work is done on the thread the UiDevToolsServer is
  // created on. If `port` is 0, the server will choose an available port. If
  // `port` is 0 and `active_port_output_directory` is present, the server will
  // write the chosen port to `kUIDevToolsActivePortFileName` on
  // `active_port_output_directory`.
  static std::unique_ptr<UiDevToolsServer> CreateForViews(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      int port,
      const base::FilePath& active_port_output_directory = base::FilePath());

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
  void SendOverWebSocket(int connection_id, std::string_view message);

  int port() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    return port_;
  }

  TracingAgent* tracing_agent() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    return tracing_agent_;
  }

  void set_tracing_agent(TracingAgent* agent) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
    tracing_agent_ = agent;
  }

  // Sets the callback which will be invoked when the session is closed.
  // Marks as a const function so it can be called after the server is set up
  // and used as a constant instance.
  void SetOnSessionEnded(base::OnceClosure callback) const;
  // Sets a callback that tests can use to wait for the server to be ready to
  // accept connections.
  void SetOnSocketConnectedForTesting(base::OnceClosure on_socket_connected);
  // Allows calling OnWebSocketRequest() with unexpected connection IDs for
  // tests, bypassing the HttpServer.
  void OnWebSocketRequestForTesting(int connection_id,
                                    net::HttpServerRequestInfo info);

 private:
  // Class that owns the ServerSocket and, after creation on the main thread,
  // may only subsequently be used on the IO thread. The class is needed because
  // HttpServers must live on an IO thread. This class contains all state that's
  // read or written on the IO thread, to avoid UAF's on teardown. When the
  // UiDevToolsServer server is destroyed, a task is posted to destroy the
  // IOThreadData, but that task could still be run after the UiDevToolsServer
  // is fully torn down.
  //
  // Because of this pattern, it's always safe to post messages to the IO thread
  // to dereference the IOThreadData from the UiDevToolsServer assuming this
  // class will still be valid, but tasks it posts back to the main thread to
  // dereference the UiDevToolsServer must use weak pointers.
  class IOThreadData;

  UiDevToolsServer(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      int port,
      const net::NetworkTrafficAnnotationTag tag,
      const base::FilePath& active_port_output_directory);

  // Invoked on the IO thread, initializes `server_` and starts listening for
  // connections on `port`.
  void MakeServer(int port);

  // Invoked on the main thread. Called by MakeServer via a post task.
  void ServerCreated(int port);

  // These mirror the corresponding HttpServer methods.
  void OnWebSocketRequest(int connection_id, net::HttpServerRequestInfo info);
  void OnWebSocketMessage(int connection_id, std::string data);
  void OnClose(int connection_id);

  using ClientsList = std::vector<std::unique_ptr<UiDevToolsClient>>;
  using ConnectionsMap =
      std::map<uint32_t, raw_ptr<UiDevToolsClient, CtnExperimental>>;
  ClientsList clients_ GUARDED_BY_CONTEXT(main_sequence_);
  ConnectionsMap connections_ GUARDED_BY_CONTEXT(main_sequence_);

  const scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  // The port the devtools server listens on.
  int port_ GUARDED_BY_CONTEXT(main_sequence_);

  raw_ptr<TracingAgent> tracing_agent_ GUARDED_BY_CONTEXT(main_sequence_) =
      nullptr;

  // Invoked when the server doesn't have any live connections.
  mutable base::OnceClosure on_session_ended_
      GUARDED_BY_CONTEXT(main_sequence_);

  // Set once the server has been started.
  bool connected_ GUARDED_BY_CONTEXT(main_sequence_) = false;

  // Invoked once the server has been started.
  base::OnceClosure on_socket_connected_ GUARDED_BY_CONTEXT(main_sequence_);

  // The server (owned by Chrome for now)
  static UiDevToolsServer* devtools_server_;

  std::unique_ptr<IOThreadData> io_thread_data_;

  SEQUENCE_CHECKER(main_sequence_);
  base::WeakPtrFactory<UiDevToolsServer> weak_ptr_factory_{this};
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_
