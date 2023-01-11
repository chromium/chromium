// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_CONNECTED_SOCKET_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_CONNECTED_SOCKET_H_

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

namespace ash {
namespace nearby {

// A trivial implementation of TCPConnectedSocket that can invoke a callback
// upon destruction. Used for unit tests.
class FakeTcpConnectedSocket : public network::mojom::TCPConnectedSocket {
 public:
  FakeTcpConnectedSocket(mojo::ScopedDataPipeProducerHandle producer_handle,
                         mojo::ScopedDataPipeConsumerHandle consumer_handle);
  ~FakeTcpConnectedSocket() override;

  void SetOnDestroyCallback(base::OnceClosure on_destroy_callback);

 private:
  // network::mojom::TCPConnectedSocket:
  void UpgradeToTLS(
      const net::HostPortPair& host_port_pair,
      network::mojom::TLSClientSocketOptionsPtr socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TLSClientSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      network::mojom::TCPConnectedSocket::UpgradeToTLSCallback callback)
      override;
  void SetSendBufferSize(int send_buffer_size,
                         SetSendBufferSizeCallback callback) override;
  void SetReceiveBufferSize(int send_buffer_size,
                            SetSendBufferSizeCallback callback) override;
  void SetNoDelay(bool no_delay, SetNoDelayCallback callback) override;
  void SetKeepAlive(bool enable,
                    int32_t delay_secs,
                    SetKeepAliveCallback callback) override;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  base::OnceClosure on_destroy_callback_;
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_CONNECTED_SOCKET_H_
