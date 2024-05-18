// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_MEDIUM_H_

#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/wifi_direct/public/mojom/wifi_direct_manager.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_direct.h"
#include "third_party/nearby/src/internal/platform/wifi_credential.h"

namespace nearby::chrome {

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
      absl::string_view ip_address,
      int port,
      CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(
      int port) override;
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
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

  void OnDisconnect();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>
      wifi_direct_manager_;
  mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_;
  mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectConnection> connection_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_MEDIUM_H_
