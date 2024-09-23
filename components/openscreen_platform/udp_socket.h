// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_UDP_SOCKET_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_UDP_SOCKET_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "third_party/openscreen/src/platform/api/udp_socket.h"
#include "third_party/openscreen/src/platform/base/error.h"
#include "third_party/openscreen/src/platform/base/interface_info.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace net {
class IPEndPoint;
}

namespace openscreen_platform {

class UdpSocket final : public openscreen::UdpSocket,
                        network::mojom::UDPSocketListener {
 public:
  UdpSocket(Client* client,
            const openscreen::IPEndpoint& local_endpoint,
            mojo::Remote<network::mojom::UDPSocket> udp_socket,
            mojo::PendingReceiver<network::mojom::UDPSocketListener>
                pending_listener);
  ~UdpSocket() final;

  // Implementations of openscreen::UdpSocket methods.
  bool IsIPv4() const final;
  bool IsIPv6() const final;
  openscreen::IPEndpoint GetLocalEndpoint() const final;
  void Bind() final;
  void SetMulticastOutboundInterface(
      openscreen::NetworkInterfaceIndex ifindex) final;
  void JoinMulticastGroup(const openscreen::IPAddress& address,
                          openscreen::NetworkInterfaceIndex ifindex) final;
  void SendMessage(openscreen::ByteView data,
                   const openscreen::IPEndpoint& dest) final;
  void SetDscp(openscreen::UdpSocket::DscpMode state) final;

  // network::mojom::UDPSocketListener overrides:
  void OnReceived(int32_t net_result,
                  const std::optional<net::IPEndPoint>& source_endpoint,
                  std::optional<base::span<const uint8_t>> data) override;

 private:
  void BindCallback(int32_t result,
                    const std::optional<net::IPEndPoint>& address);
  void JoinGroupCallback(int32_t result);
  void SendCallback(int32_t result);

  const raw_ptr<Client> client_ = nullptr;

  // The local endpoint can change as a result of bind calls.
  openscreen::IPEndpoint local_endpoint_;
  mojo::Remote<network::mojom::UDPSocket> udp_socket_;

  // The pending listener gets converted to a proper Receiver when this socket
  // gets bound.
  mojo::PendingReceiver<network::mojom::UDPSocketListener> pending_listener_;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_{this};
  base::WeakPtrFactory<UdpSocket> weak_ptr_factory_{this};
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_UDP_SOCKET_H_
