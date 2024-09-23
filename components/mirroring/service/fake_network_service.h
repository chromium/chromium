// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_
#define COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_

#include "base/functional/callback.h"
#include "media/cast/common/packet.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_udp_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

// Determine a unused UDP port.
// Method: Bind a UDP socket on port 0, and then check which port the
// operating system assigned to it.
net::IPEndPoint GetFreeLocalPort();

class MockUdpSocket final : public network::TestUDPSocket {
 public:
  MockUdpSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener);

  MockUdpSocket(const MockUdpSocket&) = delete;
  MockUdpSocket& operator=(const MockUdpSocket&) = delete;

  ~MockUdpSocket() override;

  MOCK_METHOD0(OnSend, void());
  MOCK_METHOD0(OnSendTo, void());

  // network::mojom::UDPSocket implementation.
  void Bind(const net::IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override;
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback) override;
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override;
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;

  // Simulate receiving a packet from the network.
  void OnReceivedPacket(const media::cast::Packet& packet);

  void VerifySendingPacket(const media::cast::Packet& packet);

 private:
  mojo::Receiver<network::mojom::UDPSocket> receiver_;
  mojo::Remote<network::mojom::UDPSocketListener> listener_;
  std::unique_ptr<media::cast::Packet> sending_packet_;
  int num_ask_for_receive_ = 0;
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver);

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

  ~MockNetworkContext() override;

  MOCK_METHOD0(OnUDPSocketCreated, void());

  // network::mojom::NetworkContext implementation:
  void CreateUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener) override;
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      network::mojom::URLLoaderFactoryParamsPtr params) override;

  MockUdpSocket* udp_socket() const { return udp_socket_.get(); }

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_;
  std::unique_ptr<MockUdpSocket> udp_socket_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_
