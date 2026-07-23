// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/fake_socket_factory.h"

#include <algorithm>
#include <memory>

#include "base/compiler_specific.h"
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
    listener_->OnReceived(net::OK, std::nullopt,
                          base::span<const uint8_t>(packet));
    ASSERT_LT(0, num_ask_for_receive_);
    --num_ask_for_receive_;
  }
}

void MockUdpSocket::VerifySendingPacket(const media::cast::Packet& packet) {
  EXPECT_TRUE(std::ranges::equal(packet, *sending_packet_));
}

MockSocketFactory::MockSocketFactory(
    mojo::PendingReceiver<network::mojom::SocketFactory> receiver)
    : receiver_(this, std::move(receiver)) {}
MockSocketFactory::~MockSocketFactory() = default;

void MockSocketFactory::CreateUDPSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  udp_socket_ =
      std::make_unique<MockUdpSocket>(std::move(receiver), std::move(listener));
  OnUDPSocketCreated();
}

void MockSocketFactory::CreateTCPConnectedSocket(
    const std::optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    CreateTCPConnectedSocketCallback callback) {
  std::move(callback).Run(net::ERR_FAILED, std::nullopt, std::nullopt,
                          mojo::ScopedDataPipeConsumerHandle{},
                          mojo::ScopedDataPipeProducerHandle{});
}

}  // namespace mirroring
