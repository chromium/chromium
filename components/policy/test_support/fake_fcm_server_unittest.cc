// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/fake_fcm_server.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/policy/test_support/fcm_test_util.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fakedms {

namespace {

bool ReadLoginResponse(net::StreamSocket& socket) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  net::TestCompletionCallback callback;
  if (callback.GetResult(socket.Read(buffer.get(), 1, callback.callback())) !=
      1) {
    return false;
  }
  mcs_proto::LoginResponse login_response;
  return buffer->span()[0] == kFcmProtocolVersion &&
         ReadFcmPacket(socket, gcm::kLoginResponseTag, &login_response);
}

void ExpectConnectionClosed(net::StreamSocket& socket) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(16);
  net::TestCompletionCallback callback;
  int rv = callback.GetResult(
      socket.Read(buffer.get(), buffer->size(), callback.callback()));
  EXPECT_EQ(rv, 0);
}

}  // namespace

class FakeFcmServerTest : public testing::Test {
 protected:
  std::unique_ptr<net::TCPClientSocket> ConnectRawClient() {
    auto client = std::make_unique<net::TCPClientSocket>(
        net::AddressList(
            net::IPEndPoint(net::IPAddress::IPv4Localhost(), server_.port())),
        nullptr, nullptr, nullptr, net::NetLogSource(),
        net::handles::kInvalidNetworkHandle);
    net::TestCompletionCallback callback;
    if (callback.GetResult(client->Connect(callback.callback())) != net::OK) {
      return nullptr;
    }
    return client;
  }

  std::unique_ptr<net::TCPClientSocket> ConnectAndLogin(uint64_t android_id) {
    return ConnectAndLoginFcmClient(server_.port(), android_id);
  }

  // Lets the IO loop process pending socket activity before the next action.
  void PumpIoBriefly() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(10));
    run_loop.Run();
  }

  // Calls BroadcastDataMessage() on the thread pool.
  void BroadcastFromThreadPool(const mcs_proto::DataMessageStanza& message) {
    base::test::TestFuture<void> future;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&FakeFcmServer::BroadcastDataMessage,
                       base::Unretained(&server_), message),
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Calls HasLoggedInClients() on the thread pool and returns its result.
  bool HasLoggedInClientsFromThreadPool() {
    base::test::TestFuture<bool> future;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FakeFcmServer::HasLoggedInClients,
                       base::Unretained(&server_)),
        future.GetCallback());
    return future.Get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  FakeFcmServer server_;
};

TEST_F(FakeFcmServerTest, HeartbeatPingAndAck) {
  ASSERT_TRUE(server_.Start(0));

  auto client = ConnectAndLogin(12345);
  ASSERT_TRUE(client);

  mcs_proto::HeartbeatPing ping;
  ping.set_stream_id(1);
  ASSERT_TRUE(WriteAllBytesToSocket(
      *client, EncodeFcmPacket(gcm::kHeartbeatPingTag, ping)));

  mcs_proto::HeartbeatAck ack;
  EXPECT_TRUE(ReadFcmPacket(*client, gcm::kHeartbeatAckTag, &ack));
}

TEST_F(FakeFcmServerTest, BroadcastDataMessage) {
  ASSERT_TRUE(server_.Start(0));

  mcs_proto::DataMessageStanza data_message;
  data_message.set_category("com.google.chrome.fcm");
  data_message.set_from("fake_sender");
  auto* app_data = data_message.add_app_data();
  app_data->set_key("type");
  app_data->set_value("BROADCAST");

  EXPECT_FALSE(server_.HasLoggedInClients());
  server_.BroadcastDataMessage(data_message);

  auto client1 = ConnectAndLogin(11111);
  ASSERT_TRUE(client1);
  auto client2 = ConnectAndLogin(33333);
  ASSERT_TRUE(client2);

  EXPECT_TRUE(server_.HasLoggedInClients());
  server_.BroadcastDataMessage(data_message);

  for (auto* client : {client1.get(), client2.get()}) {
    mcs_proto::DataMessageStanza received;
    ASSERT_TRUE(ReadFcmPacket(*client, gcm::kDataMessageStanzaTag, &received));
    EXPECT_EQ(received.SerializeAsString(), data_message.SerializeAsString());
  }
}

TEST_F(FakeFcmServerTest, BroadcastFromOtherSequence) {
  ASSERT_TRUE(server_.Start(0));
  mcs_proto::DataMessageStanza data_message;
  data_message.set_category("com.google.chrome.fcm");
  data_message.set_from("fake_sender");

  EXPECT_FALSE(HasLoggedInClientsFromThreadPool());
  BroadcastFromThreadPool(data_message);

  auto client = ConnectAndLogin(22222);
  ASSERT_TRUE(client);
  EXPECT_TRUE(HasLoggedInClientsFromThreadPool());
  BroadcastFromThreadPool(data_message);

  mcs_proto::DataMessageStanza received;
  ASSERT_TRUE(ReadFcmPacket(*client, gcm::kDataMessageStanzaTag, &received));
  EXPECT_EQ(received.SerializeAsString(), data_message.SerializeAsString());
}

TEST_F(FakeFcmServerTest, LoginRequestInTwoWrites) {
  ASSERT_TRUE(server_.Start(0));

  auto client = ConnectRawClient();
  ASSERT_TRUE(client);

  auto login_request = gcm::BuildLoginRequest(12345, 67890, "test-version");
  std::string packet = EncodeFcmPacket(gcm::kLoginRequestTag, *login_request,
                                       /*with_version=*/true);
  ASSERT_GT(packet.size(), 3u);

  // Send only the version, tag, and first header byte so ParseFrame() buffers.
  ASSERT_TRUE(WriteAllBytesToSocket(*client, packet.substr(0, 3)));
  PumpIoBriefly();
  EXPECT_FALSE(server_.HasLoggedInClients());

  ASSERT_TRUE(WriteAllBytesToSocket(*client, packet.substr(3)));
  ASSERT_TRUE(ReadLoginResponse(*client));
  EXPECT_TRUE(server_.HasLoggedInClients());

  mcs_proto::DataMessageStanza data_message;
  data_message.set_category("com.google.chrome.fcm");
  data_message.set_from("fake_sender");
  server_.BroadcastDataMessage(data_message);

  mcs_proto::DataMessageStanza received;
  ASSERT_TRUE(ReadFcmPacket(*client, gcm::kDataMessageStanzaTag, &received));
  EXPECT_EQ(received.SerializeAsString(), data_message.SerializeAsString());
}

TEST_F(FakeFcmServerTest, ClientDisconnectUpdatesLoggedInState) {
  ASSERT_TRUE(server_.Start(0));

  auto client = ConnectAndLogin(12345);
  ASSERT_TRUE(client);
  EXPECT_TRUE(server_.HasLoggedInClients());

  client->Disconnect();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !server_.HasLoggedInClients(); }));

  // Broadcasting with no connected clients is a safe no-op.
  server_.BroadcastDataMessage(mcs_proto::DataMessageStanza());
  EXPECT_FALSE(server_.HasLoggedInClients());
}

TEST_F(FakeFcmServerTest, UnauthenticatedClientDoesNotReceiveBroadcasts) {
  ASSERT_TRUE(server_.Start(0));

  auto client = ConnectRawClient();
  ASSERT_TRUE(client);
  PumpIoBriefly();
  EXPECT_FALSE(server_.HasLoggedInClients());

  mcs_proto::DataMessageStanza pre_login_msg;
  pre_login_msg.set_category("before_login");
  pre_login_msg.set_from("fake_sender");
  server_.BroadcastDataMessage(pre_login_msg);

  // If the pre-login broadcast was sent, ReadLoginResponse() will fail because
  // it would read a DataMessageStanza tag instead of the login response.
  auto login_request = gcm::BuildLoginRequest(12345, 67890, "test-version");
  ASSERT_TRUE(WriteAllBytesToSocket(
      *client, EncodeFcmPacket(gcm::kLoginRequestTag, *login_request,
                               /*with_version=*/true)));
  ASSERT_TRUE(ReadLoginResponse(*client));
  EXPECT_TRUE(server_.HasLoggedInClients());

  mcs_proto::DataMessageStanza post_login_msg;
  post_login_msg.set_category("after_login");
  post_login_msg.set_from("fake_sender");
  server_.BroadcastDataMessage(post_login_msg);

  mcs_proto::DataMessageStanza received;
  ASSERT_TRUE(ReadFcmPacket(*client, gcm::kDataMessageStanzaTag, &received));
  EXPECT_EQ(received.category(), "after_login");
}

TEST_F(FakeFcmServerTest, BadLoginClosesConnection) {
  ASSERT_TRUE(server_.Start(0));

  auto login_request = gcm::BuildLoginRequest(12345, 67890, "test-version");
  std::string bad_version_packet =
      std::string(1, '\x01') + EncodeFcmPacket(gcm::kLoginRequestTag,
                                               *login_request,
                                               /*with_version=*/false);
  std::string bad_tag_packet =
      EncodeFcmPacket(gcm::kHeartbeatPingTag, mcs_proto::HeartbeatPing(),
                      /*with_version=*/true);

  for (const std::string& packet : {bad_version_packet, bad_tag_packet}) {
    auto client = ConnectRawClient();
    ASSERT_TRUE(client);
    ASSERT_TRUE(WriteAllBytesToSocket(*client, packet));
    ExpectConnectionClosed(*client);
    EXPECT_FALSE(server_.HasLoggedInClients());
  }
}

}  // namespace fakedms
