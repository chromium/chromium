// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_WIFI_DIRECT_WIFI_DIRECT_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_WIFI_DIRECT_WIFI_DIRECT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"
#include "chromeos/ash/services/wifi_direct/public/mojom/wifi_direct_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

class WifiP2PGroup;

namespace wifi_direct {

class WifiDirectConnection;

// Implementation of mojom::WifiDirectManager. This class is responsible for:
// 1. create Wifi direct group.
// 2. connect to Wifi direct group.
// 3. own the WifiDirectConnection instance.
// 4. TODO: destroy Wifi direct group.
// 5. TODO: disconnect Wifi direct group.
// 6. TODO: observe on WifiP2PController to handle Wifi direct group
// disconnections notified by Shill.
class WifiDirectManager : public mojom::WifiDirectManager,
                          public WifiP2PController::Observer {
 public:
  WifiDirectManager();

  WifiDirectManager(const WifiDirectManager&) = delete;
  WifiDirectManager& operator=(const WifiDirectManager&) = delete;

  ~WifiDirectManager() override;

  // Binds a PendingReceiver to this instance. Clients wishing to use the
  // WifiDirectManager API should use this function as an entrypoint.
  void BindPendingReceiver(
      mojo::PendingReceiver<mojom::WifiDirectManager> pending_receiver);

  // mojom::WifiDirectManager
  void CreateWifiDirectGroup(wifi_direct::mojom::WifiCredentialsPtr credentials,
                             CreateWifiDirectGroupCallback callback) override;
  void ConnectToWifiDirectGroup(
      wifi_direct::mojom::WifiCredentialsPtr credentials,
      std::optional<uint32_t> frequency,
      ConnectToWifiDirectGroupCallback callback) override;

  void GetWifiP2PCapabilities(GetWifiP2PCapabilitiesCallback callback) override;

  void OnWifiDirectConnectionDisconnected(const int shill_id,
                                          bool is_owner) override;

  size_t GetConnectionsCountForTesting() const;
  void FlushForTesting();

 private:
  void OnCreateOrConnectWifiDirectGroup(
      CreateWifiDirectGroupCallback callback,
      WifiP2PController::OperationResult result,
      std::optional<WifiP2PGroup> group_metadata);
  void OnDestroyOrDisconnectWifiDirectGroup(
      WifiP2PController::OperationResult result);
  void OnClientRequestedDisconnection(int shill_id);

  mojo::ReceiverSet<mojom::WifiDirectManager> receivers_;
  base::flat_map<int, std::unique_ptr<WifiDirectConnection>>
      shill_id_to_wifi_direct_connection_;

  base::WeakPtrFactory<WifiDirectManager> weak_ptr_factory_{this};
};

}  // namespace wifi_direct

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_WIFI_DIRECT_WIFI_DIRECT_MANAGER_H_
