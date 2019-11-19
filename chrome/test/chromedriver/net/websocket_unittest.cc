// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/websocket.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/net/test_http_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

void OnConnectFinished(base::RunLoop* run_loop, int* save_error, int error) {
  *save_error = error;
  run_loop->Quit();
}

class Listener : public WebSocketListener {
 public:
  explicit Listener(const std::vector<std::string>& messages)
      : messages_(messages) {}

  ~Listener() override { EXPECT_TRUE(messages_.empty()); }

  void OnMessageReceived(const std::string& message) override {
    ASSERT_TRUE(messages_.size());
    EXPECT_EQ(messages_[0], message);
    messages_.erase(messages_.begin());
    if (messages_.empty())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  void OnClose() override { EXPECT_TRUE(false); }

 private:
  std::vector<std::string> messages_;
};

class CloseListener : public WebSocketListener {
 public:
  explicit CloseListener(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  ~CloseListener() override { EXPECT_FALSE(run_loop_); }

  void OnMessageReceived(const std::string& message) override {}

  void OnClose() override {
    EXPECT_TRUE(run_loop_);
    if (run_loop_)
      run_loop_->Quit();
    run_loop_ = NULL;
  }

 private:
  base::RunLoop* run_loop_;
};

class WebSocketTest : public testing::Test {
 public:
  WebSocketTest() {}
  ~WebSocketTest() override {}

  void SetUp() override { ASSERT_TRUE(server_.Start()); }

  void TearDown() override { server_.Stop(); }

 protected:
  std::unique_ptr<WebSocket> CreateWebSocket(const GURL& url,
                                             WebSocketListener* listener) {
    int error;
    std::unique_ptr<WebSocket> sock(
        read_buffer_size_ == 0
            ? new WebSocket(url, listener)
            : new WebSocket(url, listener, read_buffer_size_));
    base::RunLoop run_loop;
    sock->Connect(base::BindOnce(&OnConnectFinished, &run_loop, &error));
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(10));
    run_loop.Run();
    if (error == net::OK)
      return sock;
    return std::unique_ptr<WebSocket>();
  }

  std::unique_ptr<WebSocket> CreateConnectedWebSocket(
      WebSocketListener* listener) {
    return CreateWebSocket(server_.web_socket_url(), listener);
  }

  void SendReceive(const std::vector<std::string>& messages) {
    Listener listener(messages);
    std::unique_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
    ASSERT_TRUE(sock);
    for (size_t i = 0; i < messages.size(); ++i) {
      ASSERT_TRUE(sock->Send(messages[i]));
    }
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(10));
    run_loop.Run();
  }

  void SetReadBufferSize(size_t size) { read_buffer_size_ = size; }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  TestHttpServer server_;
  size_t read_buffer_size_ = 0;
};

}  // namespace

TEST_F(WebSocketTest, CreateDestroy) {
  CloseListener listener(NULL);
  WebSocket sock(GURL("ws://127.0.0.1:2222"), &listener);
}

TEST_F(WebSocketTest, Connect) {
  CloseListener listener(NULL);
  ASSERT_TRUE(CreateWebSocket(server_.web_socket_url(), &listener));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(server_.WaitForConnectionsToClose());
}

TEST_F(WebSocketTest, ConnectNoServer) {
  CloseListener listener(NULL);
  ASSERT_FALSE(CreateWebSocket(GURL("ws://127.0.0.1:33333"), NULL));
}

TEST_F(WebSocketTest, Connect404) {
  server_.SetRequestAction(TestHttpServer::kNotFound);
  CloseListener listener(NULL);
  ASSERT_FALSE(CreateWebSocket(server_.web_socket_url(), NULL));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(server_.WaitForConnectionsToClose());
}

TEST_F(WebSocketTest, ConnectServerClosesConn) {
  server_.SetRequestAction(TestHttpServer::kClose);
  CloseListener listener(NULL);
  ASSERT_FALSE(CreateWebSocket(server_.web_socket_url(), &listener));
}

TEST_F(WebSocketTest, CloseOnReceive) {
  server_.SetMessageAction(TestHttpServer::kCloseOnMessage);
  base::RunLoop run_loop;
  CloseListener listener(&run_loop);
  std::unique_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
  ASSERT_TRUE(sock);
  ASSERT_TRUE(sock->Send("hi"));
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(10));
  run_loop.Run();
}

TEST_F(WebSocketTest, CloseOnSend) {
  base::RunLoop run_loop;
  CloseListener listener(&run_loop);
  std::unique_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
  ASSERT_TRUE(sock);
  server_.Stop();

  sock->Send("hi");
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(10));
  run_loop.Run();
  ASSERT_FALSE(sock->Send("hi"));
}

#if !DCHECK_IS_ON()
TEST_F(WebSocketTest, SendReceive) {
  std::vector<std::string> messages;
  messages.push_back("hello");
  SendReceive(messages);
}
#endif

#if !DCHECK_IS_ON()
TEST_F(WebSocketTest, SendReceiveLarge) {
  std::vector<std::string> messages;
  messages.push_back(std::string(10 << 20, 'a'));
  SendReceive(messages);
}
#endif

#if !DCHECK_IS_ON()
TEST_F(WebSocketTest, SendReceiveManyPacks) {
  std::vector<std::string> messages;
  // A message size of 1 << 16 crashes code with https://crbug.com/877105 bug
  // on Linux and Windows, but a size of 1 << 17 is needed to cause crash on
  // Mac. We use message size 1 << 18 for some extra margin to ensure bug repro.
  messages.push_back(std::string(1 << 18, 'a'));
  SetReadBufferSize(1);
  SendReceive(messages);
}
#endif

#if !DCHECK_IS_ON()
TEST_F(WebSocketTest, SendReceiveMultiple) {
  std::vector<std::string> messages;
  messages.push_back("1");
  messages.push_back("2");
  messages.push_back("3");
  SendReceive(messages);
}
#endif
