// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_socket.h"

#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/platform/bidirectional_stream.h"

namespace nearby::chrome {

WifiLanSocket::ConnectedSocketParameters::ConnectedSocketParameters(
    mojo::PendingRemote<network::mojom::TCPConnectedSocket>
        tcp_connected_socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream)
    : tcp_connected_socket(std::move(tcp_connected_socket)),
      receive_stream(std::move(receive_stream)),
      send_stream(std::move(send_stream)) {}

WifiLanSocket::ConnectedSocketParameters::~ConnectedSocketParameters() =
    default;

WifiLanSocket::ConnectedSocketParameters::ConnectedSocketParameters(
    ConnectedSocketParameters&&) = default;

WifiLanSocket::ConnectedSocketParameters&
WifiLanSocket::ConnectedSocketParameters::operator=(
    ConnectedSocketParameters&&) = default;

WifiLanSocket::WifiLanSocket(
    ConnectedSocketParameters connected_socket_parameters)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      tcp_connected_socket_(
          std::move(connected_socket_parameters.tcp_connected_socket),
          task_runner_),
      bidirectional_stream_(
          connections::mojom::Medium::kWifiLan,
          task_runner_,
          std::move(connected_socket_parameters.receive_stream),
          std::move(connected_socket_parameters.send_stream)) {
  tcp_connected_socket_.set_disconnect_handler(
      base::BindOnce(&WifiLanSocket::OnTcpConnectedSocketDisconnected,
                     base::Unretained(this)),
      task_runner_);
}

WifiLanSocket::~WifiLanSocket() {
  Close();
}

InputStream& WifiLanSocket::GetInputStream() {
  DCHECK(bidirectional_stream_.GetInputStream());
  return *bidirectional_stream_.GetInputStream();
}

OutputStream& WifiLanSocket::GetOutputStream() {
  DCHECK(bidirectional_stream_.GetOutputStream());
  return *bidirectional_stream_.GetOutputStream();
}

// Note: Both CloseTcpSocketIfNecessary() and BidirectionalStream::Close() are
// thread safe.
Exception WifiLanSocket::Close() {
  CloseTcpSocketIfNecessary();

  return bidirectional_stream_.Close();
}

bool WifiLanSocket::IsClosed() const {
  return !tcp_connected_socket_.is_bound();
}

void WifiLanSocket::OnTcpConnectedSocketDisconnected() {
  LOG(WARNING) << "WifiLanSocket::" << __func__
               << ": TCP connected socket unexpectedly disconnected. Closing "
               << "WifiLanSocket.";
  Close();
}

void WifiLanSocket::CloseTcpSocketIfNecessary() {
  base::AutoLock lock(lock_);

  if (!tcp_connected_socket_)
    return;

  VLOG(1) << "WifiLanSocket::" << __func__ << ": Closing TCP connected socket.";
  tcp_connected_socket_.reset();
}

}  // namespace nearby::chrome
