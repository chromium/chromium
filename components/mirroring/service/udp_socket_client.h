// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_UDP_SOCKET_CLIENT_H_
#define COMPONENTS_MIRRORING_SERVICE_UDP_SOCKET_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "media/cast/net/cast_transport_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace mirroring {

// This class implements a UDP packet transport that can send/receive UDP
// packets to/from |remote_endpoint_| through a UDPSocket that implements
// network::mojom::UDPSocket. The UDPSocket is created and connected to
// |remote_endpoint_| when StartReceiving() is called. Sending/Receiving ends
// when StopReceiving() is called or this class is destructed. |error_callback|
// will be called if the UDPSocket is failed to be created or connected.
class COMPONENT_EXPORT(MIRRORING_SERVICE) UdpSocketClient final
    : public media::cast::PacketTransport,
      public network::mojom::UDPSocketListener {
 public:
  UdpSocketClient(const net::IPEndPoint& remote_endpoint,
                  network::mojom::NetworkContext* context,
                  base::OnceClosure error_callback);

  ~UdpSocketClient() override;

  // media::cast::PacketTransport implementations.
  bool SendPacket(media::cast::PacketRef packet,
                  const base::RepeatingClosure& cb) override;
  int64_t GetBytesSent() override;
  void StartReceiving(const media::cast::PacketReceiverCallbackWithStatus&
                          packet_receiver) override;
  void StopReceiving() override;

  // network::mojom::UDPSocketListener implementation.
  void OnReceived(int32_t result,
                  const base::Optional<net::IPEndPoint>& src_addr,
                  base::Optional<base::span<const uint8_t>> data) override;

 private:
  // The callback of network::mojom::UDPSocket::Send(). Further sending is
  // blocked if |result| indicates that pending send requests buffer is full.
  void OnPacketSent(int result);

  // The callback of network::mojom::UDPSocket::Connect(). Packets are only
  // allowed to send after the socket is successfully connected to the
  // |remote_endpoint_|.
  void OnSocketConnected(int result,
                         const base::Optional<net::IPEndPoint>& addr);

  const net::IPEndPoint remote_endpoint_;
  network::mojom::NetworkContext* const network_context_;
  base::OnceClosure error_callback_;

  mojo::Receiver<network::mojom::UDPSocketListener> receiver_{this};

  // The callback to deliver the received packets to the packet parser. Set
  // when StartReceiving() is called.
  media::cast::PacketReceiverCallbackWithStatus packet_receiver_callback_;

  mojo::Remote<network::mojom::UDPSocket> udp_socket_;

  // Set by SendPacket() when the sending is not allowed. Once set, SendPacket()
  // can only be called again when a previous sending completes successfully.
  // TODO(crbug.com/1015479): Change the callback to a base::OnceClosure as well
  // as in the cast::PacketTransport SendPacket().
  base::RepeatingClosure resume_send_callback_;

  // Total numbe of bytes written to the data pipe.
  int64_t bytes_sent_;

  // Indicates whether further sending is allowed. Set to true when the
  // UDPSocket is successfully connected to |remote_endpoint_|, and false when
  // too many send requests are pending.
  bool allow_sending_;

  // The number of packets pending to receive.
  int num_packets_pending_receive_;

  base::WeakPtrFactory<UdpSocketClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UdpSocketClient);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_UDP_SOCKET_CLIENT_H_
