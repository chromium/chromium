// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_SOCKET_FACTORY_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_SOCKET_FACTORY_H_

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace ash {
namespace nearby {

// An implementation of TcpSocketFactory used for unit tests. The user sets
// expectations--via SetCreate{Server,Connected}SocketCallExpectations()--for
// the number of CreateTCP{Server,Connected}Socket() calls that will be queued
// up. The user is notified when all calls are queued. The user sequentially
// processes the callbacks in the queue via
// FinishNextCreate{Server,Connected}Socket(). On success,
// FakeTcp{Server,Connected}Sockets are returned.
class FakeTcpSocketFactory : public sharing::mojom::TcpSocketFactory {
 public:
  explicit FakeTcpSocketFactory(const net::IPEndPoint& default_local_addr);
  ~FakeTcpSocketFactory() override;
  FakeTcpSocketFactory(const FakeTcpSocketFactory&) = delete;
  FakeTcpSocketFactory& operator=(const FakeTcpSocketFactory&) = delete;

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

  // sharing::mojom::TcpSocketFactory:
  void CreateTCPServerSocket(
      const net::IPAddress& local_addr,
      const ash::nearby::TcpServerSocketPort& port,
      uint32_t backlog,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPServerSocket> socket,
      CreateTCPServerSocketCallback callback) override;
  void CreateTCPConnectedSocket(
      base::TimeDelta timeout,
      const std::optional<net::IPEndPoint>& local_addr,
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

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_TCP_SOCKET_FACTORY_H_
