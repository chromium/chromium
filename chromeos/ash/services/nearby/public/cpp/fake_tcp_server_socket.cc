// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_server_socket.h"

#include <memory>

#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_connected_socket.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"

namespace ash {
namespace nearby {

FakeTcpServerSocket::FakeTcpServerSocket() = default;

FakeTcpServerSocket::~FakeTcpServerSocket() = default;

void FakeTcpServerSocket::SetAcceptCallExpectations(
    size_t expected_num_accept_calls,
    base::OnceClosure on_all_accept_calls_queued) {
  expected_num_accept_calls_ = expected_num_accept_calls;
  if (expected_num_accept_calls == 0) {
    std::move(on_all_accept_calls_queued).Run();
  } else {
    on_all_accept_calls_queued_ = std::move(on_all_accept_calls_queued);
  }
}

void FakeTcpServerSocket::FinishNextAccept(
    int32_t net_error,
    const std::optional<::net::IPEndPoint>& remote_addr) {
  DCHECK(!pending_accept_callbacks_.empty());
  AcceptCallback callback = std::move(pending_accept_callbacks_.front());
  pending_accept_callbacks_.pop_front();

  if (net_error != net::OK) {
    std::move(callback).Run(
        net_error, /*remote_addr=*/std::nullopt,
        /*connected_socket=*/mojo::NullRemote(),
        /*receive_stream=*/mojo::ScopedDataPipeConsumerHandle(),
        /*send_stream=*/mojo::ScopedDataPipeProducerHandle());
    return;
  }

  DCHECK(remote_addr);

  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
  DCHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                 receive_pipe_producer_handle,
                                                 receive_pipe_consumer_handle));
  mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
  DCHECK_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/*options=*/nullptr, send_pipe_producer_handle,
                                 send_pipe_consumer_handle));
  mojo::PendingRemote<network::mojom::TCPConnectedSocket>
      pending_tcp_connected_socket;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeTcpConnectedSocket>(
          std::move(receive_pipe_producer_handle),
          std::move(send_pipe_consumer_handle)),
      pending_tcp_connected_socket.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(net_error, remote_addr,
                          std::move(pending_tcp_connected_socket),
                          std::move(receive_pipe_consumer_handle),
                          std::move(send_pipe_producer_handle));
}

void FakeTcpServerSocket::Accept(
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    AcceptCallback callback) {
  pending_accept_callbacks_.push_back(std::move(callback));
  DCHECK_GE(expected_num_accept_calls_, pending_accept_callbacks_.size());

  if (pending_accept_callbacks_.size() == expected_num_accept_calls_) {
    DCHECK(on_all_accept_calls_queued_);
    std::move(on_all_accept_calls_queued_).Run();
  }
}

}  // namespace nearby
}  // namespace ash
