// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_connected_socket.h"

#include "base/notreached.h"

namespace ash {
namespace nearby {

FakeTcpConnectedSocket::FakeTcpConnectedSocket(
    mojo::ScopedDataPipeProducerHandle producer_handle,
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : producer_handle_(std::move(producer_handle)),
      consumer_handle_(std::move(consumer_handle)) {}

FakeTcpConnectedSocket::~FakeTcpConnectedSocket() {
  if (on_destroy_callback_)
    std::move(on_destroy_callback_).Run();
}

void FakeTcpConnectedSocket::SetOnDestroyCallback(
    base::OnceClosure on_destroy_callback) {
  on_destroy_callback_ = std::move(on_destroy_callback);
}

void FakeTcpConnectedSocket::UpgradeToTLS(
    const net::HostPortPair& host_port_pair,
    network::mojom::TLSClientSocketOptionsPtr socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TLSClientSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    network::mojom::TCPConnectedSocket::UpgradeToTLSCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTcpConnectedSocket::SetSendBufferSize(
    int send_buffer_size,
    SetSendBufferSizeCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTcpConnectedSocket::SetReceiveBufferSize(
    int send_buffer_size,
    SetSendBufferSizeCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTcpConnectedSocket::SetNoDelay(bool no_delay,
                                        SetNoDelayCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTcpConnectedSocket::SetKeepAlive(bool enable,
                                          int32_t delay_secs,
                                          SetKeepAliveCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace nearby
}  // namespace ash
