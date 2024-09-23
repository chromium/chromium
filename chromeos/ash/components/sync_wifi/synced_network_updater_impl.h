// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/pending_network_configuration_tracker.h"
#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/ash/components/sync_wifi/synced_network_updater.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::timer_factory {
class TimerFactory;
}  // namespace ash::timer_factory

namespace ash::sync_wifi {

// Implementation of SyncedNetworkUpdater. This class takes add/update/delete
// requests from the sync backend and applies them to the local network stack
// using mojom::CrosNetworkConfig.
class SyncedNetworkUpdaterImpl
    : public SyncedNetworkUpdater,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  // |cros_network_config| must outlive this class.
  SyncedNetworkUpdaterImpl(
      std::unique_ptr<PendingNetworkConfigurationTracker> tracker,
      chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config,
      ash::timer_factory::TimerFactory* timer_factory,
      SyncedNetworkMetricsLogger* metrics_logger);
  ~SyncedNetworkUpdaterImpl() override;

  void AddOrUpdateNetwork(
      const sync_pb::WifiConfigurationSpecifics& specifics) override;

  void RemoveNetwork(const NetworkIdentifier& id) override;

  bool IsUpdateInProgress(const std::string& network_guid) override;

  // CrosNetworkConfigObserver:
  void OnNetworkStateListChanged() override;

 private:
  void StartAddOrUpdateOperation(
      const std::string& change_guid,
      const NetworkIdentifier& id,
      const sync_pb::WifiConfigurationSpecifics& specifics);
  void StartDeleteOperation(const std::string& change_guid,
                            const NetworkIdentifier& id,
                            std::string guid);
  void StartTimer(const std::string& change_guid, const NetworkIdentifier& id);
  void Retry(const PendingNetworkConfigurationUpdate& update);
  void HandleShillResult(const std::string& change_guid,
                         const NetworkIdentifier& id,
                         bool is_success);
  void CleanupUpdate(const std::string& change_guid,
                     const NetworkIdentifier& id);
  chromeos::network_config::mojom::NetworkStatePropertiesPtr FindMojoNetwork(
      const NetworkIdentifier& id);

  void OnGetNetworkList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void OnTimeout(const std::string& change_guid, const NetworkIdentifier& id);
  void OnSetPropertiesResult(const std::string& change_guid,
                             const std::string& network_guid,
                             const sync_pb::WifiConfigurationSpecifics& proto,
                             bool success,
                             const std::string& error_message);
  void OnConfigureNetworkResult(
      const std::string& change_guid,
      const sync_pb::WifiConfigurationSpecifics& proto,
      const std::optional<std::string>& network_guid,
      const std::string& error_message);
  void OnForgetNetworkResult(const std::string& change_guid,
                             const NetworkIdentifier& id,
                             bool success);

  std::unique_ptr<PendingNetworkConfigurationTracker> tracker_;
  raw_ptr<chromeos::network_config::mojom::CrosNetworkConfig, DanglingUntriaged>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
      networks_;
  raw_ptr<ash::timer_factory::TimerFactory> timer_factory_;
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      change_guid_to_timer_map_;
  base::flat_map<std::string, int> network_guid_to_updates_counter_;
  raw_ptr<SyncedNetworkMetricsLogger> metrics_logger_;

  base::WeakPtrFactory<SyncedNetworkUpdaterImpl> weak_ptr_factory_{this};
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_IMPL_H_
