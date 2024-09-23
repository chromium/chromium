// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_MEDIUM_H_

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/wifi_direct/public/mojom/wifi_direct_manager.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_direct.h"
#include "third_party/nearby/src/internal/platform/wifi_credential.h"

namespace nearby::chrome {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WifiDirectServiceError {
  kNoConnection = 0,
  kFailedToCreatePlatformSocket = 1,
  kFailedToAssociateSocket = 2,
  kCancelled = 3,
  kFailedToConnectSocket = 4,
  kFailedToOpenFirewallHole = 5,
  kFailedToListenToSocket = 6,
  kMaxValue = kFailedToListenToSocket,
};

class WifiDirectMedium : public api::WifiDirectMedium {
 public:
  explicit WifiDirectMedium(
      const mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>&
          manager,
      const mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>&
          firewall_hole_factory);
  ~WifiDirectMedium() override;

  // api::WifiDirectMedium
  bool IsInterfaceValid() const override;
  bool StartWifiDirect(WifiDirectCredentials* credentials) override;
  bool StopWifiDirect() override;
  bool ConnectWifiDirect(WifiDirectCredentials* credentials) override;
  bool DisconnectWifiDirect() override;
  std::unique_ptr<api::WifiDirectSocket> ConnectToService(
      std::string_view ip_address,
      int port,
      CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(
      int port) override;
  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override;

 private:
  void GetCapabilities(bool* is_capability_supported,
                       base::WaitableEvent* waitable_event) const;
  void OnCapabilities(
      bool* is_capability_supported,
      base::WaitableEvent* waitable_event,
      ash::wifi_direct::mojom::WifiP2PCapabilitiesPtr capabilities) const;

  void CreateGroup(WifiDirectCredentials* credentials,
                   base::WaitableEvent* waitable_event);
  void OnGroupCreated(
      WifiDirectCredentials* credentials,
      base::WaitableEvent* waitable_event,
      ash::wifi_direct::mojom::WifiDirectOperationResult result,
      mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectConnection>
          connection);
  void OnProperties(
      WifiDirectCredentials* credentials,
      base::WaitableEvent* waitable_event,
      ash::wifi_direct::mojom::WifiDirectConnectionPropertiesPtr properties);

  void ConnectGroup(WifiDirectCredentials* credentials,
                    base::WaitableEvent* waitable_event);
  void OnGroupConnected(
      base::WaitableEvent* waitable_event,
      ash::wifi_direct::mojom::WifiDirectOperationResult result,
      mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectConnection>
          connection);

  void AssociateSocket(bool* did_associate,
                       base::WaitableEvent* waitable_event,
                       mojo::PlatformHandle socket_descriptor);
  void OnSocketAssociated(bool* did_associate,
                          base::WaitableEvent* waitable_event,
                          bool success);
  void CreateAndListenToSocket(int16_t port,
                               net::SocketDescriptor socket_descriptor,
                               std::unique_ptr<net::TCPServerSocket>* socket,
                               base::WaitableEvent* waitable_event);
  void CreateAndConnectSocket(const std::string_view& ip_address,
                              int port,
                              int fd,
                              std::unique_ptr<net::TCPClientSocket>* socket,
                              base::WaitableEvent* waitable_event);
  void OnSocketConnected(std::unique_ptr<net::TCPClientSocket>* socket,
                         base::WaitableEvent* waitable_event,
                         std::unique_ptr<net::TCPSocket> tcp_socket,
                         net::IPEndPoint ip_endpoint,
                         int result);

  void OpenFirewallHole(
      ash::nearby::TcpServerSocketPort port,
      mojo::PendingRemote<::sharing::mojom::FirewallHole>* output,
      base::WaitableEvent* waitable_event);
  void OnFirewallHoleCreated(
      mojo::PendingRemote<::sharing::mojom::FirewallHole>* output,
      base::WaitableEvent* waitable_event,
      mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole);

  void OnDisconnect();

  std::unique_ptr<base::Thread> io_thread_;
  mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>
      wifi_direct_manager_;
  mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_;
  mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectConnection> connection_;
  std::string ipv4_address_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_MEDIUM_H_
