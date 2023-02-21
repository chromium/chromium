// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/udp_socket_client.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/mirroring/service/fake_network_service.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::Packet;
using ::testing::InvokeWithoutArgs;

namespace mirroring {

class UdpSocketClientTest : public ::testing::Test {
 public:
  UdpSocketClientTest() {
    network_context_ = std::make_unique<MockNetworkContext>(
        network_context_remote_.BindNewPipeAndPassReceiver());
    udp_transport_client_ = std::make_unique<UdpSocketClient>(
        media::cast::test::GetFreeLocalPort(), network_context_remote_.get(),
        base::OnceClosure());
  }

  UdpSocketClientTest(const UdpSocketClientTest&) = delete;
  UdpSocketClientTest& operator=(const UdpSocketClientTest&) = delete;

  ~UdpSocketClientTest() override = default;

  MOCK_METHOD0(OnReceivedPacketCall, void());
  bool OnReceivedPacket(std::unique_ptr<Packet> packet) {
    received_packet_ = std::move(packet);
    OnReceivedPacketCall();
    return true;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<MockNetworkContext> network_context_;
  std::unique_ptr<UdpSocketClient> udp_transport_client_;
  std::unique_ptr<Packet> received_packet_;
};

TEST_F(UdpSocketClientTest, SendAndReceive) {
  std::string data = "Test";
  Packet packet(data.begin(), data.end());

  {
    // Expect the UDPSocket to be created when calling StartReceiving().
    base::RunLoop run_loop;
    EXPECT_CALL(*network_context_, OnUDPSocketCreated())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    udp_transport_client_->StartReceiving(base::BindRepeating(
        &UdpSocketClientTest::OnReceivedPacket, base::Unretained(this)));
    run_loop.Run();
  }
  task_environment_.RunUntilIdle();

  MockUdpSocket* socket = network_context_->udp_socket();

  {
    // Request to send one packet.
    base::RunLoop run_loop;
    base::OnceClosure cb;
    EXPECT_CALL(*socket, OnSend())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    EXPECT_TRUE(udp_transport_client_->SendPacket(
        new base::RefCountedData<Packet>(packet), std::move(cb)));
    run_loop.Run();
  }

  // Expect the packet to be sent is delivered to the UDPSocket.
  socket->VerifySendingPacket(packet);

  // Test receiving packet.
  std::string data2 = "Hello";
  Packet packet2(data.begin(), data.end());
  {
    // Simulate receiving |packet2| from the network.
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnReceivedPacketCall())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    socket->OnReceivedPacket(packet2);
    run_loop.Run();
  }

  // The packet is expected to be received.
  EXPECT_TRUE(base::ranges::equal(packet2, *received_packet_));

  udp_transport_client_->StopReceiving();
}

TEST_F(UdpSocketClientTest, SendBeforeConnected) {
  std::string data = "Test";
  Packet packet(data.begin(), data.end());

  // Request to send one packet.
  base::MockCallback<base::OnceClosure> resume_send_cb;
  {
    EXPECT_CALL(resume_send_cb, Run()).Times(0);
    EXPECT_FALSE(udp_transport_client_->SendPacket(
        new base::RefCountedData<Packet>(packet), resume_send_cb.Get()));
    task_environment_.RunUntilIdle();
  }
  {
    // Expect the UDPSocket to be created when calling StartReceiving().
    base::RunLoop run_loop;
    EXPECT_CALL(*network_context_, OnUDPSocketCreated()).Times(1);
    EXPECT_CALL(resume_send_cb, Run())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    udp_transport_client_->StartReceiving(base::BindRepeating(
        &UdpSocketClientTest::OnReceivedPacket, base::Unretained(this)));
    run_loop.Run();
  }
  udp_transport_client_->StopReceiving();
}

}  // namespace mirroring
