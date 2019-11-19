// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_IMPL_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/pending_network_configuration_tracker.h"
#include "chromeos/components/sync_wifi/synced_network_updater.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

namespace sync_wifi {

// Implementation of SyncedNetworkUpdater. This class takes add/update/delete
// requests from the sync backend and applies them to the local network stack
// using chromeos::NetworkConfigurationHandler.
class SyncedNetworkUpdaterImpl
    : public SyncedNetworkUpdater,
      public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  // |cros_network_config| must outlive this class.
  SyncedNetworkUpdaterImpl(
      std::unique_ptr<PendingNetworkConfigurationTracker> tracker,
      network_config::mojom::CrosNetworkConfig* cros_network_config);
  ~SyncedNetworkUpdaterImpl() override;

  void AddOrUpdateNetwork(
      const sync_pb::WifiConfigurationSpecificsData& specifics) override;

  void RemoveNetwork(const NetworkIdentifier& id) override;

  // CrosNetworkConfigObserver:
  void OnNetworkStateListChanged() override;
  void OnActiveNetworksChanged(
      std::vector<
          network_config::mojom::NetworkStatePropertiesPtr> /* networks */)
      override {}
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr /* network */)
      override {}
  void OnDeviceStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}

 private:
  void CleanupUpdate(const std::string& change_guid,
                     const NetworkIdentifier& id);
  network_config::mojom::NetworkStatePropertiesPtr FindLocalNetwork(
      const NetworkIdentifier& id);

  base::Optional<base::DictionaryValue> ConvertToDictionary(
      const sync_pb::WifiConfigurationSpecificsData& specifics,
      const std::string& guid);
  void OnGetNetworkList(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks);
  void OnError(const std::string& change_guid,
               const NetworkIdentifier& id,
               const std::string& error_name);
  void OnSetPropertiesResult(const std::string& change_guid,
                             const NetworkIdentifier& id,
                             bool success,
                             const std::string& error_message);
  void OnConfigureNetworkResult(const std::string& change_guid,
                                const NetworkIdentifier& id,
                                const base::Optional<std::string>& guid,
                                const std::string& error_message);
  void OnForgetNetworkResult(const std::string& change_guid,
                             const NetworkIdentifier& id,
                             bool success);

  std::unique_ptr<PendingNetworkConfigurationTracker> tracker_;
  network_config::mojom::CrosNetworkConfig* cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
  std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks_;

  base::WeakPtrFactory<SyncedNetworkUpdaterImpl> weak_ptr_factory_{this};
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_IMPL_H_
