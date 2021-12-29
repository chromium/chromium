// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_MEDIUM_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/services/sharing/nearby/platform/wifi_lan_server_socket.h"
#include "chrome/services/sharing/nearby/platform/wifi_lan_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "third_party/nearby/src/cpp/platform/api/wifi_lan.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace location {
namespace nearby {
namespace chrome {

// An implementation of the abstract Nearby Connections's class
// api::WifiLanMedium. The implementation uses the network services's
// NetworkContext mojo interface to 1) connect to remote server sockets, and 2)
// open local server sockets to listen for incoming connection requests from
// remote devices. We block while 1) trying to connect, 2) creating a server
// socket, and 3) cancelling pending tasks in the destructor. We guarantee
// thread safety, and we guarantee that all blocking connection and listening
// attempts return before destruction.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  explicit WifiLanMedium(
      const mojo::SharedRemote<network::mojom::NetworkContext>&
          network_context);
  WifiLanMedium(const WifiLanMedium&) = delete;
  WifiLanMedium& operator=(const WifiLanMedium&) = delete;
  ~WifiLanMedium() override;

  // api::WifiLanMedium:
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string& ip_address,
      int port,
      CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanServerSocket> ListenForService(int port) override;

 private:
  /*==========================================================================*/
  // ConnectToService() helpers: Connect to remote server socket.
  /*==========================================================================*/
  void DoConnect(const net::AddressList& address_list,
                 absl::optional<WifiLanSocket::ConnectedSocketParameters>*
                     connected_socket_parameters,
                 base::WaitableEvent* connect_waitable_event);
  void OnConnect(absl::optional<WifiLanSocket::ConnectedSocketParameters>*
                     connected_socket_parameters,
                 base::WaitableEvent* connect_waitable_event,
                 mojo::PendingRemote<network::mojom::TCPConnectedSocket>
                     tcp_connected_socket,
                 int32_t result,
                 const absl::optional<net::IPEndPoint>& local_addr,
                 const absl::optional<net::IPEndPoint>& peer_addr,
                 mojo::ScopedDataPipeConsumerHandle receive_stream,
                 mojo::ScopedDataPipeProducerHandle send_stream);
  /*==========================================================================*/

  /*==========================================================================*/
  // ListenForService() helpers: Listen for and accept incoming connections.
  /*==========================================================================*/
  void DoListenForService(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      int port);
  void OnLocalIpAddressFetched(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      const net::IPEndPoint& local_end_point);
  void OnTcpServerSocketCreated(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
      int32_t result,
      const absl::optional<net::IPEndPoint>& local_addr);
  // TODO(https://crbug.com/1261238): Add firewall hole PendingRemote argument.
  void OnFirewallHoleCreated(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
      const absl::optional<net::IPEndPoint>& local_addr);
  /*==========================================================================*/

  /*==========================================================================*/
  // api::WifiLanMedium: Not implemented.
  /*==========================================================================*/
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override;
  bool StopDiscovery(const std::string& service_type) override;
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override;
  /*==========================================================================*/

  // Removes |event| from the set of pending events and signals |event|. Calls
  // to these methods are sequenced on |task_runner_| and thus thread safe.
  void FinishConnectAttempt(base::WaitableEvent* event);
  void FinishListenAttempt(base::WaitableEvent* event);

  // Resets the |network_context_| and finishes all pending connect/listen
  // attempts with null results.
  void Shutdown(base::WaitableEvent* shutdown_waitable_event);

  // TODO(https://crbug.com/1261238): Add firewall hole factory.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::SharedRemote<network::mojom::NetworkContext> network_context_;

  // Track all pending connect/listen tasks in case Close() is called while
  // waiting.
  base::flat_set<base::WaitableEvent*> pending_connect_waitable_events_;
  base::flat_set<base::WaitableEvent*> pending_listen_waitable_events_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_MEDIUM_H_
