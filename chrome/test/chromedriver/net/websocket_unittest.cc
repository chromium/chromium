// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/websocket.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/net/test_http_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

void OnConnectFinished(base::RunLoop* run_loop, int* save_error, int error) {
  *save_error = error;
  run_loop->Quit();
}

class Listener : public WebSocketListener {
 public:
  explicit Listener(const std::vector<std::string>& messages,
                    base::RunLoop* run_loop)
      : messages_(messages), run_loop_(run_loop) {}

  ~Listener() override { EXPECT_TRUE(messages_.empty()); }

  void OnMessageReceived(const std::string& message) override {
    ASSERT_TRUE(messages_.size());
    EXPECT_EQ(messages_[0], message);
    messages_.erase(messages_.begin());
    if (messages_.empty())
      run_loop_->QuitWhenIdle();
  }

  void OnClose() override { EXPECT_TRUE(false); }

 private:
  std::vector<std::string> messages_;
  raw_ptr<base::RunLoop> run_loop_;
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
    run_loop_ = nullptr;
  }

 private:
  raw_ptr<base::RunLoop> run_loop_;
};

class MessageReceivedListener : public WebSocketListener {
 public:
  MessageReceivedListener() = default;
  ~MessageReceivedListener() override = default;

  void OnMessageReceived(const std::string& message) override {
    messages_.push_back(message);
  }

  const std::vector<std::string>& Messages() const { return messages_; }

  void OnClose() override {}

 private:
  std::vector<std::string> messages_;
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
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
    run_loop.Run();
    if (error == net::OK)
      return sock;
    return nullptr;
  }

  std::unique_ptr<WebSocket> CreateConnectedWebSocket(
      WebSocketListener* listener) {
    return CreateWebSocket(server_.web_socket_url(), listener);
  }

  void SendReceive(const std::vector<std::string>& messages) {
    base::RunLoop run_loop;
    Listener listener(messages, &run_loop);
    std::unique_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
    ASSERT_TRUE(sock);
    for (size_t i = 0; i < messages.size(); ++i) {
      ASSERT_TRUE(sock->Send(messages[i]));
    }
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
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
  CloseListener listener(nullptr);
  WebSocket sock(GURL("ws://127.0.0.1:2222"), &listener);
}

TEST_F(WebSocketTest, Connect) {
  CloseListener listener(nullptr);
  ASSERT_TRUE(CreateWebSocket(server_.web_socket_url(), &listener));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(server_.WaitForConnectionsToClose());
}

TEST_F(WebSocketTest, ConnectNoServer) {
  CloseListener listener(nullptr);
  ASSERT_FALSE(CreateWebSocket(GURL("ws://127.0.0.1:33333"), nullptr));
}

TEST_F(WebSocketTest, Connect404) {
  server_.SetRequestAction(TestHttpServer::kNotFound);
  CloseListener listener(nullptr);
  ASSERT_FALSE(CreateWebSocket(server_.web_socket_url(), nullptr));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(server_.WaitForConnectionsToClose());
}

TEST_F(WebSocketTest, ConnectServerClosesConn) {
  server_.SetRequestAction(TestHttpServer::kClose);
  CloseListener listener(nullptr);
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
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
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
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
  run_loop.Run();
  ASSERT_FALSE(sock->Send("hi"));
}

TEST_F(WebSocketTest, SendReceive) {
  std::vector<std::string> messages;
  messages.push_back("hello");
  SendReceive(messages);
}

TEST_F(WebSocketTest, SendReceiveLarge) {
  std::vector<std::string> messages;
  messages.push_back(std::string(10 << 20, 'a'));
  SendReceive(messages);
}

TEST_F(WebSocketTest, SendReceiveManyPacks) {
  std::vector<std::string> messages;
  // A message size of 1 << 16 crashes code with https://crbug.com/877105 bug
  // on Linux and Windows, but a size of 1 << 17 is needed to cause crash on
  // Mac. We use message size 1 << 18 for some extra margin to ensure bug repro.
  messages.push_back(std::string(1 << 18, 'a'));
  SetReadBufferSize(1);
  SendReceive(messages);
}

TEST_F(WebSocketTest, SendReceiveMultiple) {
  std::vector<std::string> messages;
  messages.push_back("1");
  messages.push_back("2");
  messages.push_back("3");
  SendReceive(messages);
}

TEST_F(WebSocketTest, VerifyTextFramelsProcessed) {
  constexpr uint8_t kFinalBit = 0x80;

  const std::string kOriginalMessage = "hello";
  std::string frame = {
      static_cast<char>(net::WebSocketFrameHeader::kOpCodeText | kFinalBit),
      static_cast<char>(kOriginalMessage.length())};
  frame += kOriginalMessage;
  std::string encoded_frame = base::Base64Encode(frame);

  server_.SetMessageAction(TestHttpServer::kEchoRawMessage);
  base::RunLoop run_loop;
  MessageReceivedListener listener;
  std::unique_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
  ASSERT_TRUE(sock);
  ASSERT_TRUE(sock->Send(encoded_frame));

  EXPECT_EQ(listener.Messages().size(), 0u);
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
  run_loop.Run();
  EXPECT_THAT(listener.Messages(), testing::ElementsAre(kOriginalMessage));
}

TEST_F(WebSocketTest, VerifyBinaryFramelsNotProcessed) {
  constexpr uint8_t kFinalBit = 0x80;

  const std::string kOriginalMessage = "hello";
  std::string frame = {
      static_cast<char>(net::WebSocketFrameHeader::kOpCodeBinary | kFinalBit),
      static_cast<char>(kOriginalMessage.length())};
  frame += kOriginalMessage;
  std::string encoded_frame = base::Base64Encode(frame);

  server_.SetMessageAction(TestHttpServer::kEchoRawMessage);
  base::RunLoop run_loop;
  MessageReceivedListener listener;
  std::unique_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
  ASSERT_TRUE(sock);
  ASSERT_TRUE(sock->Send(encoded_frame));
  EXPECT_EQ(listener.Messages().size(), 0u);

  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
  run_loop.Run();
  EXPECT_EQ(listener.Messages().size(), 0u);
}

TEST_F(WebSocketTest, VerifyCloseFramelsNotProcessed) {
  constexpr uint8_t kFinalBit = 0x80;

  std::string frame = {
      static_cast<char>(net::WebSocketFrameHeader::kOpCodeClose | kFinalBit),
      0};
  std::string encoded_frame = base::Base64Encode(frame);

  server_.SetMessageAction(TestHttpServer::kEchoRawMessage);
  base::RunLoop run_loop;
  MessageReceivedListener listener;
  std::unique_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
  ASSERT_TRUE(sock);
  ASSERT_TRUE(sock->Send(encoded_frame));
  EXPECT_EQ(listener.Messages().size(), 0u);

  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
  run_loop.Run();
  EXPECT_EQ(listener.Messages().size(), 0u);
}
