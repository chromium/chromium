// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/test_http_server.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class SyncWebSocketImplTest : public testing::Test {
 protected:
  SyncWebSocketImplTest()
      : client_thread_("ClientThread"), long_timeout_(base::Minutes(1)) {}
  ~SyncWebSocketImplTest() override {}

  void SetUp() override {
    ASSERT_TRUE(client_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));
    context_getter_ = new URLRequestContextGetter(client_thread_.task_runner());
    ASSERT_TRUE(server_.Start());
  }

  void TearDown() override { server_.Stop(); }

  Timeout long_timeout() const { return Timeout(long_timeout_); }

  base::test::SingleThreadTaskEnvironment task_environment;
  base::Thread client_thread_;
  TestHttpServer server_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  const base::TimeDelta long_timeout_;
};

}  // namespace

TEST_F(SyncWebSocketImplTest, CreateDestroy) {
  SyncWebSocketImpl sock(context_getter_.get());
}

// TODO(crbug.com/40168673) Re-enable test
TEST_F(SyncWebSocketImplTest, DISABLED_Connect) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
}

TEST_F(SyncWebSocketImplTest, ConnectFail) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_FALSE(sock.Connect(GURL("ws://127.0.0.1:33333")));
}

TEST_F(SyncWebSocketImplTest, SendReceive) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("hi"));
  std::string message;
  ASSERT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));
  ASSERT_STREQ("hi", message.c_str());
}

TEST_F(SyncWebSocketImplTest, DetermineRecipient) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  std::string message_for_chromedriver = R"({
        "id": 1,
        "method": "Page.enable"
      })";
  std::string message_not_for_chromedriver = R"({
        "id": -1,
        "method": "Page.enable"
      })";
  sock.Send(message_not_for_chromedriver);
  sock.Send(message_for_chromedriver);
  std::string message;
  ASSERT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));

  // Getting message id and method
  std::optional<base::Value> message_value = base::JSONReader::Read(message);
  ASSERT_TRUE(message_value.has_value());
  base::Value::Dict* message_dict = message_value->GetIfDict();
  ASSERT_TRUE(message_dict);
  const std::string* method = message_dict->FindString("method");
  ASSERT_EQ(*method, "Page.enable");
  int id = message_dict->FindInt("id").value_or(-1);
  ASSERT_EQ(id, 1);
}

TEST_F(SyncWebSocketImplTest, SendReceiveTimeout) {
  SyncWebSocketImpl sock(context_getter_.get());

  // The server might reply too quickly so that the response will be received
  // before we call ReceiveNextMessage; we must prevent it.
  base::WaitableEvent server_reply_allowed(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  server_.SetMessageCallback(base::BindOnce(
      &base::WaitableEvent::Wait, base::Unretained(&server_reply_allowed)));

  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("hi"));
  std::string message;
  ASSERT_EQ(SyncWebSocket::StatusCode::kTimeout,
            sock.ReceiveNextMessage(&message, Timeout(base::TimeDelta())));

  server_reply_allowed.Signal();
  // Receive the response to avoid possible deletion of the event while the
  // server thread has not yet returned from the call to Wait.
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));
}

TEST_F(SyncWebSocketImplTest, SendReceiveLarge) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  std::string wrote_message(10 << 20, 'a');
  ASSERT_TRUE(sock.Send(wrote_message));
  std::string message;
  ASSERT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));
  ASSERT_EQ(wrote_message.length(), message.length());
  ASSERT_EQ(wrote_message, message);
}

TEST_F(SyncWebSocketImplTest, SendReceiveMany) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("1"));
  ASSERT_TRUE(sock.Send("2"));
  std::string message;
  ASSERT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));
  ASSERT_STREQ("1", message.c_str());
  ASSERT_TRUE(sock.Send("3"));
  ASSERT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));
  ASSERT_STREQ("2", message.c_str());
  ASSERT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));
  ASSERT_STREQ("3", message.c_str());
}

TEST_F(SyncWebSocketImplTest, CloseOnReceive) {
  server_.SetMessageAction(TestHttpServer::kCloseOnMessage);
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("1"));
  std::string message;
  ASSERT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            sock.ReceiveNextMessage(&message, long_timeout()));
  ASSERT_STREQ("", message.c_str());
}

TEST_F(SyncWebSocketImplTest, CloseOnSend) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  server_.Stop();
  ASSERT_FALSE(sock.Send("1"));
}

TEST_F(SyncWebSocketImplTest, Reconnect) {
  SyncWebSocketImpl sock(context_getter_.get());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("1"));
  // Wait for SyncWebSocket to receive the response from the server.
  Timeout response_timeout(base::Seconds(20));
  while (!response_timeout.IsExpired()) {
    if (sock.IsConnected() && !sock.HasNextMessage())
      base::PlatformThread::Sleep(base::Milliseconds(10));
    else
      break;
  }
  server_.Stop();
  ASSERT_FALSE(sock.Send("2"));
  ASSERT_FALSE(sock.IsConnected());
  server_.Start();
  ASSERT_TRUE(sock.HasNextMessage());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_FALSE(sock.HasNextMessage());
  ASSERT_TRUE(sock.Send("3"));
  std::string message;
  ASSERT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));
  ASSERT_STREQ("3", message.c_str());
  ASSERT_FALSE(sock.HasNextMessage());
}

TEST_F(SyncWebSocketImplTest, NotificationArrives) {
  base::RunLoop run_loop;
  SyncWebSocketImpl sock(context_getter_.get());
  bool notified = false;

  sock.SetNotificationCallback(base::BindRepeating(
      [](bool& flag, base::RepeatingClosure callback) {
        flag = true;
        callback.Run();
      },
      std::ref(notified), run_loop.QuitClosure()));

  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("there"));
  std::string message;

  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            sock.ReceiveNextMessage(&message, long_timeout()));

  // Notification must arrive via the message queue.
  // If it arrives earlier then we have a threading problem.
  EXPECT_FALSE(notified);

  run_loop.Run();

  EXPECT_TRUE(notified);
}
