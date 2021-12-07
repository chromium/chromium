// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_FAKE_NETWORK_CONTEXT_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_FAKE_NETWORK_CONTEXT_H_

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "net/base/ip_endpoint.h"
#include "services/network/test/test_network_context.h"

namespace location {
namespace nearby {
namespace chrome {

// An implementation of NetworkContext used for unit tests. The user sets
// expectations--via SetCreate{Server,Connected}SocketCallExpectations()--for
// the number of CreateTCP{Server,Connected}Socket() calls that will be queued
// up. The user is notified when all calls are queued. The user sequentially
// processes the callbacks in the queue via
// FinishNextCreate{Server,Connected}Socket(). On success,
// FakeTcp{Server,Connected}Sockets are returned.
class FakeNetworkContext : public network::TestNetworkContext {
 public:
  explicit FakeNetworkContext(const net::IPEndPoint& default_local_addr);
  ~FakeNetworkContext() override;
  FakeNetworkContext(const FakeNetworkContext&) = delete;
  FakeNetworkContext& operator=(const FakeNetworkContext&) = delete;

  void SetCreateServerSocketCallExpectations(
      size_t expected_num_create_server_socket_calls,
      base::OnceClosure on_all_create_server_socket_calls_queued);
  void SetCreateConnectedSocketCallExpectations(
      size_t expected_num_create_connected_socket_calls,
      base::OnceClosure on_all_create_connected_socket_calls_queued);

  void FinishNextCreateServerSocket(int32_t result);
  void FinishNextCreateConnectedSocket(int32_t result);

 private:
  using CreateCallback = base::OnceCallback<void(int32_t result)>;

  // network::TestNetworkContext:
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      uint32_t backlog,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPServerSocket> socket,
      CreateTCPServerSocketCallback callback) override;
  void CreateTCPConnectedSocket(
      const absl::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override;

  net::IPEndPoint default_local_addr_;
  size_t expected_num_create_server_socket_calls_ = 0;
  size_t expected_num_create_connected_socket_calls_ = 0;
  base::OnceClosure on_all_create_server_socket_calls_queued_;
  base::OnceClosure on_all_create_connected_socket_calls_queued_;
  base::circular_deque<CreateCallback> pending_create_server_socket_callbacks_;
  base::circular_deque<CreateCallback>
      pending_create_connected_socket_callbacks_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_FAKE_NETWORK_CONTEXT_H_
