// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/libcast_socket_service.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/openscreen_platform/network_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"
#include "third_party/openscreen/src/cast/common/channel/testing/fake_cast_socket.h"

namespace cast_channel {
namespace {

using ::testing::_;

void OpenCallback(base::RepeatingClosure cb,
                  CastSocket** socket_save,
                  CastSocket* socket) {
  *socket_save = socket;
  cb.Run();
}

}  // namespace

class LibcastSocketServiceTest : public ::testing::Test {
 protected:
  CastSocket* OpenSocket(std::unique_ptr<LibcastSocket> cast_socket,
                         const net::IPEndPoint& endpoint) {
    socket_service_.SetLibcastSocketForTest(std::move(cast_socket));
    CastSocketOpenParams open_params(endpoint, base::Seconds(20));
    CastSocket* socket = nullptr;
    base::RunLoop run_loop;
    socket_service_.OpenSocket(
        base::BindRepeating(
            []() -> network::mojom::NetworkContext* { return nullptr; }),
        open_params,
        base::BindOnce(&OpenCallback, run_loop.QuitClosure(),
                       base::Unretained(&socket)));

    run_loop.Run();
    return socket;
  }

  content::BrowserTaskEnvironment task_environment_;
  LibcastSocketService socket_service_;
};

TEST_F(LibcastSocketServiceTest, ChannelAddAndRemove) {
  openscreen::cast::FakeCastSocketPair pair1({{10, 0, 0, 7}, 2001},
                                             {{10, 0, 0, 20}, 9000});
  openscreen::cast::FakeCastSocketPair pair2({{10, 0, 0, 8}, 2002},
                                             {{10, 0, 0, 21}, 9001});
  CastSocket* socket_ptr1 =
      OpenSocket(std::move(pair1.socket),
                 openscreen_platform::ToNetEndPoint(pair1.remote_endpoint));
  ASSERT_TRUE(socket_ptr1);
  CastSocket* socket_ptr2 =
      OpenSocket(std::move(pair2.socket),
                 openscreen_platform::ToNetEndPoint(pair2.remote_endpoint));
  ASSERT_TRUE(socket_ptr2);
  EXPECT_NE(socket_ptr1, socket_ptr2);
  EXPECT_NE(socket_ptr1->id(), socket_ptr2->id());

  std::unique_ptr<CastSocket> socket1 =
      socket_service_.RemoveSocket(socket_ptr1->id());
  EXPECT_EQ(socket1.get(), socket_ptr1);

  CastSocket* socket_ptr3 = OpenSocket(
      nullptr, openscreen_platform::ToNetEndPoint(pair2.remote_endpoint));
  ASSERT_TRUE(socket_ptr3);
  EXPECT_EQ(socket_ptr3, socket_ptr2);
}

TEST_F(LibcastSocketServiceTest, OpenChannelIsConnected) {
  MockCastSocketObserver mock_observer;
  socket_service_.AddObserver(&mock_observer);

  openscreen::cast::FakeCastSocketPair fake_cast_socket_pair;
  CastSocket* socket = OpenSocket(std::move(fake_cast_socket_pair.socket),
                                  openscreen_platform::ToNetEndPoint(
                                      fake_cast_socket_pair.remote_endpoint));

  base::RunLoop run_loop1;
  EXPECT_CALL(fake_cast_socket_pair.mock_peer_client, OnMessage(_, _))
      .WillOnce([&run_loop1](LibcastSocket* socket,
                             cast::channel::CastMessage message) {
        EXPECT_EQ(message.source_id(), "sender1");
        EXPECT_EQ(message.destination_id(), "receiver1");
        EXPECT_EQ(message.namespace_(), "ns1");
        ASSERT_EQ(message.payload_type(),
                  cast::channel::CastMessage_PayloadType_STRING);
        EXPECT_EQ(message.payload_utf8(), "PING");
        run_loop1.Quit();
      });
  cast::channel::CastMessage ping_message;
  ping_message.set_protocol_version(
      cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  ping_message.set_source_id("sender1");
  ping_message.set_destination_id("receiver1");
  ping_message.set_namespace_("ns1");
  ping_message.set_payload_type(cast::channel::CastMessage_PayloadType_STRING);
  ping_message.set_payload_utf8("PING");
  socket->transport()->SendMessage(ping_message, base::DoNothing());

  run_loop1.Run();

  base::RunLoop run_loop2;
  EXPECT_CALL(mock_observer, OnMessage(_, _))
      .WillOnce(
          [&run_loop2](const CastSocket& socket, const CastMessage& message) {
            EXPECT_EQ(message.source_id(), "receiver1");
            EXPECT_EQ(message.destination_id(), "sender1");
            EXPECT_EQ(message.namespace_(), "ns1");
            ASSERT_EQ(message.payload_type(),
                      cast::channel::CastMessage_PayloadType_STRING);
            EXPECT_EQ(message.payload_utf8(), "PONG");
            run_loop2.Quit();
          });
  cast::channel::CastMessage pong_message;
  pong_message.set_protocol_version(
      cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  pong_message.set_source_id("receiver1");
  pong_message.set_destination_id("sender1");
  pong_message.set_namespace_("ns1");
  pong_message.set_payload_type(cast::channel::CastMessage_PayloadType_STRING);
  pong_message.set_payload_utf8("PONG");
  ASSERT_TRUE(fake_cast_socket_pair.peer_socket->Send(pong_message).ok());

  run_loop2.Run();
}

}  // namespace cast_channel
