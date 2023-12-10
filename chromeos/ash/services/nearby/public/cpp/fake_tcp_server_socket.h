// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_SERVER_SOCKET_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_SERVER_SOCKET_H_

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace ash {
namespace nearby {

// An implementation of TCPServerSocket used for unit tests. The user sets
// expectations--via SetAcceptCallExpectations()--for the number of Accept()
// calls that will be queued up, and the user is notified when all calls are
// queued. The user sequentially processes the AcceptCallbacks in the queue via
// FinishNextAccept(). On success, FakeTcpConnectedSockets are provided to the
// AcceptCallback.
class FakeTcpServerSocket : public network::mojom::TCPServerSocket {
 public:
  FakeTcpServerSocket();
  ~FakeTcpServerSocket() override;

  size_t num_pending_accept_callbacks() const {
    return pending_accept_callbacks_.size();
  }

  // Must be set before any Accept() calls are made. Ensures that no more than
  // |expected_num_accept_calls| Accept() calls are made. When
  // |expected_num_accept_calls| Accept() calls are made,
  // |on_all_accept_calls_queued| is invoked. If |expected_num_accept_calls| is
  // 0, then |on_all_accept_calls_queued| is invoked immediately.
  void SetAcceptCallExpectations(size_t expected_num_accept_calls,
                                 base::OnceClosure on_all_accept_calls_queued);

  // Process the next AcceptCallback in |pending_accept_callbacks_|. For a
  // |net_error| of net::OK, a non-trivial |remote_addr| must be provided, and a
  // FakeTcpConnectedSocket will be used in the AcceptCallback. For errors, null
  // or trivial values are used for the AcceptCallback.
  void FinishNextAccept(int32_t net_error,
                        const std::optional<::net::IPEndPoint>& remote_addr);

 private:
  // network::mojom::TCPServerSocket:
  void Accept(mojo::PendingRemote<network::mojom::SocketObserver> observer,
              AcceptCallback callback) override;

  size_t expected_num_accept_calls_ = 0;
  base::OnceClosure on_all_accept_calls_queued_;
  base::circular_deque<AcceptCallback> pending_accept_callbacks_;
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_SERVER_SOCKET_H_
