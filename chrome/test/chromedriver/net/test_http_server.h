// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_TEST_HTTP_SERVER_H_
#define CHROME_TEST_CHROMEDRIVER_NET_TEST_HTTP_SERVER_H_

#include <set>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "net/server/http_server.h"
#include "url/gurl.h"

namespace base {
class WaitableEvent;
}

// HTTP server for web socket testing purposes that runs on its own thread.
// All public methods are thread safe and may be called on any thread, unless
// noted otherwise.
class TestHttpServer : public net::HttpServer::Delegate {
 public:
  enum WebSocketRequestAction {
    kAccept,
    kNotFound,
    kClose,
  };

  enum WebSocketMessageAction {
    kEchoMessage,
    kCloseOnMessage,
    kEchoRawMessage
  };

  // Creates an http server. By default it accepts WebSockets and echoes
  // WebSocket messages back.
  TestHttpServer();

  TestHttpServer(const TestHttpServer&) = delete;
  TestHttpServer& operator=(const TestHttpServer&) = delete;

  ~TestHttpServer() override;

  // Starts the server. Returns whether it was started successfully.
  bool Start();

  // Stops the server. May be called multiple times.
  void Stop();

  // Waits until all open connections are closed. Returns true if all
  // connections are closed, or false if a timeout is exceeded.
  bool WaitForConnectionsToClose();

  // Sets the action to perform when receiving a WebSocket connect request.
  void SetRequestAction(WebSocketRequestAction action);

  // Sets the action to perform when receiving a WebSocket message.
  void SetMessageAction(WebSocketMessageAction action);

  // Sets a callback to be called once when receiving next WebSocket message.
  void SetMessageCallback(base::OnceClosure callback);

  GURL http_url() const;

  // Returns the web socket URL that points to the server.
  GURL web_socket_url() const;

  // Overridden from net::HttpServer::Delegate:
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

  void SetDataForPath(std::string path, std::string data);

 private:
  void StartOnServerThread(bool* success, base::WaitableEvent* event);
  void StopOnServerThread(base::WaitableEvent* event);
  void SetDataForPathOnServerThread(std::string path,
                                    std::string data,
                                    base::WaitableEvent* event);

  base::Thread thread_;

  // Access only on the server thread.
  std::unique_ptr<net::HttpServer> server_;

  // Access only on the server thread.
  std::set<int> connections_;

  base::WaitableEvent all_closed_event_;

  // Protects |web_socket_url_|.
  mutable base::Lock url_lock_;
  GURL http_url_;
  GURL web_socket_url_;

  // Protects the action flags and |message_callback_|.
  base::Lock action_lock_;
  WebSocketRequestAction request_action_ = kAccept;
  WebSocketMessageAction message_action_ = kEchoMessage;
  base::OnceClosure message_callback_;

  std::map<std::string, std::string> resource_map_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_TEST_HTTP_SERVER_H_
