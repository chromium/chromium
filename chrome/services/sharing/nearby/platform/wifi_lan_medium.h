// Copyright 2021 The Chromium Authors
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
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_lan.h"

namespace ash {
namespace nearby {
class TcpServerSocketPort;
}  // namespace nearby
}  // namespace ash

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace nearby {
namespace chrome {

// An implementation of the abstract Nearby Connections's class
// api::WifiLanMedium. The implementation uses the
// sharing::mojom::TcpSocketFactory mojo interface to 1) connect to remote
// server sockets, and 2) open local server sockets to listen for incoming
// connection requests from remote devices. We block while 1) trying to connect,
// 2) creating a server socket, and 3) cancelling pending tasks in the
// destructor. We guarantee thread safety, and we guarantee that all blocking
// connection and listening attempts return before destruction.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium(const mojo::SharedRemote<sharing::mojom::TcpSocketFactory>&
                    socket_factory,
                const mojo::SharedRemote<
                    chromeos::network_config::mojom::CrosNetworkConfig>&
                    cros_network_config,
                const mojo::SharedRemote<sharing::mojom::FirewallHoleFactory>&
                    firewall_hole_factory);
  WifiLanMedium(const WifiLanMedium&) = delete;
  WifiLanMedium& operator=(const WifiLanMedium&) = delete;
  ~WifiLanMedium() override;

  // Check if a network connection to a primary router exist.
  bool IsNetworkConnected() const override;

  // api::WifiLanMedium:
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string& ip_address,
      int port,
      CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanServerSocket> ListenForService(int port) override;
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override;

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ConnectResult {
    kSuccess = 0,
    kCanceled = 1,
    kErrorFailedToCreateTcpSocket = 2,
    kMaxValue = kErrorFailedToCreateTcpSocket,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ListenResult {
    kSuccess = 0,
    kCanceled = 1,
    kErrorInvalidPort = 2,
    kErrorFetchIpFailedToGetNetworkStateList = 3,
    kErrorFetchIpFailedToGetManagedProperties = 4,
    kErrorFetchIpMissingIpConfigs = 5,
    kErrorFetchIpNoValidLocalIpAddress = 6,
    kErrorFailedToCreateTcpServerSocket = 7,
    kErrorUnexpectedTcpServerSocketIpEndpoint = 8,
    kErrorFailedToCreateFirewallHole = 9,
    kMaxValue = kErrorFailedToCreateFirewallHole,
  };

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
  void OnGetNetworkStateList(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      const ash::nearby::TcpServerSocketPort& port,
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          result);
  void OnGetNetworkProperties(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      const ash::nearby::TcpServerSocketPort& port,
      chromeos::network_config::mojom::ManagedPropertiesPtr properties);
  void OnTcpServerSocketCreated(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
      const net::IPAddress& ip_address,
      const ash::nearby::TcpServerSocketPort& port,
      int32_t result,
      const absl::optional<net::IPEndPoint>& local_addr);
  void OnFirewallHoleCreated(
      absl::optional<WifiLanServerSocket::ServerSocketParameters>*
          server_socket_parameters,
      base::WaitableEvent* listen_waitable_event,
      mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
      const net::IPEndPoint& local_addr,
      mojo::PendingRemote<sharing::mojom::FirewallHole> firewall_hole);
  /*==========================================================================*/

  /*==========================================================================*/
  // api::WifiLanMedium: Not implemented.
  /*==========================================================================*/
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override;
  bool StopDiscovery(const std::string& service_type) override;
  /*==========================================================================*/

  // Removes |event| from the set of pending events and signals |event|. Calls
  // to these methods are sequenced on |task_runner_| and thus thread safe.
  void FinishConnectAttempt(base::WaitableEvent* event, ConnectResult result);
  void FinishListenAttempt(base::WaitableEvent* event, ListenResult result);

  // Resets the SharedRemotes and finishes all pending connect/listen attempts
  // with null results.
  void Shutdown(base::WaitableEvent* shutdown_waitable_event);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::SharedRemote<sharing::mojom::TcpSocketFactory> socket_factory_;
  mojo::SharedRemote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::SharedRemote<sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_;

  // Track all pending connect/listen tasks in case Close() is called while
  // waiting.
  base::flat_set<base::WaitableEvent*> pending_connect_waitable_events_;
  base::flat_set<base::WaitableEvent*> pending_listen_waitable_events_;
};

}  // namespace chrome
}  // namespace nearby

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_MEDIUM_H_
