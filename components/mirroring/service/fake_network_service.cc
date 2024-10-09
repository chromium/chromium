// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/mirroring/service/fake_network_service.h"

#include <memory>

#include "base/ranges/algorithm.h"
// #include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/test/test_url_loader_factory.h"

namespace mirroring {

net::IPEndPoint GetFreeLocalPort() {
  std::unique_ptr<net::UDPServerSocket> receive_socket(
      new net::UDPServerSocket(nullptr, net::NetLogSource()));
  receive_socket->AllowAddressReuse();
  CHECK_EQ(net::OK, receive_socket->Listen(
                        net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)));
  net::IPEndPoint endpoint;
  CHECK_EQ(net::OK, receive_socket->GetLocalAddress(&endpoint));
  return endpoint;
}

MockUdpSocket::MockUdpSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener)
    : receiver_(this, std::move(receiver)), listener_(std::move(listener)) {}

MockUdpSocket::~MockUdpSocket() = default;

void MockUdpSocket::Bind(const net::IPEndPoint& local_addr,
                         network::mojom::UDPSocketOptionsPtr options,
                         BindCallback callback) {
  std::move(callback).Run(net::OK, GetFreeLocalPort());
}

void MockUdpSocket::Connect(const net::IPEndPoint& remote_addr,
                            network::mojom::UDPSocketOptionsPtr options,
                            ConnectCallback callback) {
  std::move(callback).Run(net::OK, GetFreeLocalPort());
}

void MockUdpSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  num_ask_for_receive_ += num_additional_datagrams;
}

void MockUdpSocket::SendTo(
    const net::IPEndPoint& dest_addr,
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  sending_packet_ =
      std::make_unique<media::cast::Packet>(data.begin(), data.end());
  std::move(callback).Run(net::OK);
  OnSendTo();
}

void MockUdpSocket::Send(
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendCallback callback) {
  sending_packet_ =
      std::make_unique<media::cast::Packet>(data.begin(), data.end());
  std::move(callback).Run(net::OK);
  OnSend();
}

void MockUdpSocket::OnReceivedPacket(const media::cast::Packet& packet) {
  if (num_ask_for_receive_) {
    listener_->OnReceived(
        net::OK, std::nullopt,
        base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(packet.data()), packet.size()));
    ASSERT_LT(0, num_ask_for_receive_);
    --num_ask_for_receive_;
  }
}

void MockUdpSocket::VerifySendingPacket(const media::cast::Packet& packet) {
  EXPECT_TRUE(base::ranges::equal(packet, *sending_packet_));
}

MockNetworkContext::MockNetworkContext(
    mojo::PendingReceiver<network::mojom::NetworkContext> receiver)
    : receiver_(this, std::move(receiver)) {}
MockNetworkContext::~MockNetworkContext() = default;

void MockNetworkContext::CreateUDPSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  udp_socket_ =
      std::make_unique<MockUdpSocket>(std::move(receiver), std::move(listener));
  OnUDPSocketCreated();
}

void MockNetworkContext::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    network::mojom::URLLoaderFactoryParamsPtr params) {
  ASSERT_TRUE(params);
  mojo::MakeSelfOwnedReceiver(std::make_unique<network::TestURLLoaderFactory>(),
                              std::move(receiver));
}

}  // namespace mirroring
