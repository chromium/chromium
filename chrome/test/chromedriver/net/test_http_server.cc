// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/test_http_server.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kBufferSize = 100 * 1024 * 1024;  // 100 MB

TestHttpServer::TestHttpServer()
    : thread_("ServerThread"),
      all_closed_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::SIGNALED) {}

TestHttpServer::~TestHttpServer() = default;

bool TestHttpServer::Start() {
  bool thread_started = thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  EXPECT_TRUE(thread_started);
  if (!thread_started)
    return false;
  bool success;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestHttpServer::StartOnServerThread,
                                base::Unretained(this), &success, &event));
  event.Wait();
  return success;
}

void TestHttpServer::Stop() {
  if (!thread_.IsRunning())
    return;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestHttpServer::StopOnServerThread,
                                base::Unretained(this), &event));
  event.Wait();
  thread_.Stop();
}

bool TestHttpServer::WaitForConnectionsToClose() {
  return all_closed_event_.TimedWait(base::Seconds(10));
}

void TestHttpServer::SetRequestAction(WebSocketRequestAction action) {
  base::AutoLock lock(action_lock_);
  request_action_ = action;
}

void TestHttpServer::SetMessageAction(WebSocketMessageAction action) {
  base::AutoLock lock(action_lock_);
  message_action_ = action;
}

void TestHttpServer::SetMessageCallback(base::OnceClosure callback) {
  base::AutoLock lock(action_lock_);
  message_callback_ = std::move(callback);
}

GURL TestHttpServer::http_url() const {
  base::AutoLock lock(url_lock_);
  return http_url_;
}

GURL TestHttpServer::web_socket_url() const {
  base::AutoLock lock(url_lock_);
  return web_socket_url_;
}

void TestHttpServer::OnConnect(int connection_id) {
  server_->SetSendBufferSize(connection_id, kBufferSize);
  server_->SetReceiveBufferSize(connection_id, kBufferSize);
}

void TestHttpServer::OnHttpRequest(int connection_id,
                                   const net::HttpServerRequestInfo& info) {
  auto it = resource_map_.find(info.path);
  if (it == resource_map_.end()) {
    server_->Send404(connection_id, TRAFFIC_ANNOTATION_FOR_TESTS);
    return;
  }
  server_->Send200(connection_id, it->second, "text/html",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
}

void TestHttpServer::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  WebSocketRequestAction action;
  {
    base::AutoLock lock(action_lock_);
    action = request_action_;
  }
  connections_.insert(connection_id);
  all_closed_event_.Reset();

  switch (action) {
    case kAccept:
      server_->AcceptWebSocket(connection_id, info,
                               TRAFFIC_ANNOTATION_FOR_TESTS);
      break;
    case kNotFound:
      server_->Send404(connection_id, TRAFFIC_ANNOTATION_FOR_TESTS);
      break;
    case kClose:
      server_->Close(connection_id);
      break;
  }
}

void TestHttpServer::OnWebSocketMessage(int connection_id, std::string data) {
  WebSocketMessageAction action;
  {
    base::AutoLock lock(action_lock_);
    action = message_action_;
  }
  if (!message_callback_.is_null())
    std::move(message_callback_).Run();
  switch (action) {
    case kEchoMessage:
      server_->SendOverWebSocket(connection_id, data,
                                 TRAFFIC_ANNOTATION_FOR_TESTS);
      break;

    case kCloseOnMessage:
      server_->Close(connection_id);
      break;

    case kEchoRawMessage:
      std::string decoded_data;
      base::Base64Decode(data, &decoded_data);
      server_->SendRaw(connection_id, decoded_data,
                       TRAFFIC_ANNOTATION_FOR_TESTS);
  }
}

void TestHttpServer::OnClose(int connection_id) {
  connections_.erase(connection_id);
  if (connections_.empty())
    all_closed_event_.Signal();
}

void TestHttpServer::StartOnServerThread(bool* success,
                                         base::WaitableEvent* event) {
  std::unique_ptr<net::ServerSocket> server_socket(
      new net::TCPServerSocket(nullptr, net::NetLogSource()));
  server_socket->ListenWithAddressAndPort("127.0.0.1", 0, 1);
  server_ = std::make_unique<net::HttpServer>(std::move(server_socket), this);

  net::IPEndPoint address;
  int error = server_->GetLocalAddress(&address);
  EXPECT_EQ(net::OK, error);
  if (error == net::OK) {
    base::AutoLock lock(url_lock_);
    http_url_ = GURL(base::StringPrintf("http://127.0.0.1:%d", address.port()));
    web_socket_url_ = GURL(base::StringPrintf("ws://127.0.0.1:%d",
                                              address.port()));
  } else {
    server_.reset();
  }
  *success = server_.get();
  event->Signal();
}

void TestHttpServer::StopOnServerThread(base::WaitableEvent* event) {
  server_.reset();
  event->Signal();
}

void TestHttpServer::SetDataForPath(std::string path, std::string data) {
  if (!thread_.IsRunning()) {
    return;
  }
  if (!base::StartsWith(path, "/")) {
    path = "/" + path;
  }
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestHttpServer::SetDataForPathOnServerThread,
                                base::Unretained(this), std::move(path),
                                std::move(data), &event));
  event.Wait();
}

void TestHttpServer::SetDataForPathOnServerThread(std::string path,
                                                  std::string data,
                                                  base::WaitableEvent* event) {
  resource_map_[std::move(path)] = std::move(data);
  event->Signal();
}
