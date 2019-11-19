// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_
#define COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_

#include "base/callback.h"
#include "media/cast/net/cast_transport_defines.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

class MockUdpSocket final : public network::mojom::UDPSocket {
 public:
  MockUdpSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener);
  ~MockUdpSocket() override;

  MOCK_METHOD0(OnSend, void());

  // network::mojom::UDPSocket implementation.
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback) override;
  void Bind(const net::IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override {}
  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override {}
  void SetSendBufferSize(int32_t send_buffer_size,
                         SetSendBufferSizeCallback callback) override {}
  void SetReceiveBufferSize(int32_t receive_buffer_size,
                            SetReceiveBufferSizeCallback callback) override {}
  void JoinGroup(const net::IPAddress& group_address,
                 JoinGroupCallback callback) override {}
  void LeaveGroup(const net::IPAddress& group_address,
                  LeaveGroupCallback callback) override {}
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override {}
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override {}
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;
  void Close() override {}

  // Simulate receiving a packet from the network.
  void OnReceivedPacket(const media::cast::Packet& packet);

  void VerifySendingPacket(const media::cast::Packet& packet);

 private:
  mojo::Receiver<network::mojom::UDPSocket> receiver_;
  mojo::Remote<network::mojom::UDPSocketListener> listener_;
  std::unique_ptr<media::cast::Packet> sending_packet_;
  int num_ask_for_receive_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockUdpSocket);
};

class MockNetworkContext final : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver);
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
  DISALLOW_COPY_AND_ASSIGN(MockNetworkContext);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_
