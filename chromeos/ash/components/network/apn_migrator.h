// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_APN_MIGRATOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_APN_MIGRATOR_H_

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class ManagedCellularPrefHandler;
class ManagedNetworkConfigurationHandler;
class NetworkMetadataStore;
class NetworkStateHandler;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) ApnMigrator
    : public NetworkStateHandlerObserver {
 public:
  ApnMigrator(
      ManagedCellularPrefHandler* managed_cellular_pref_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkStateHandler* network_state_handler,
      NetworkMetadataStore* network_metadata_store);
  ApnMigrator() = delete;
  ApnMigrator(const ApnMigrator&) = delete;
  ApnMigrator& operator=(const ApnMigrator&) = delete;
  ~ApnMigrator() override;

 private:
  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;

  // Creates an ONC configuration object for the Shill property user_apn_list
  // containing |apn_list|, and applies it for the cellular |network|.
  void SetShillUserApnListForNetwork(const NetworkState& network,
                                     const base::Value::List* apn_list);

  // Migrate the |network|'s custom APNs to the APN Revamp feature. If the
  // migration requires the network's managed properties, this function will
  // invoke an async call, and mark the network as "in migration".
  void MigrateNetwork(const NetworkState& network);

  // Finishes the migration process for networks that require managed properties
  // fields.
  void OnGetManagedProperties(std::string iccid,
                              const std::string& service_path,
                              absl::optional<base::Value> properties,
                              absl::optional<std::string> error);

  base::flat_set<std::string> iccids_in_migration_;

  ManagedCellularPrefHandler* managed_cellular_pref_handler_ = nullptr;
  ManagedNetworkConfigurationHandler* network_configuration_handler_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkMetadataStore* network_metadata_store_ = nullptr;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  base::WeakPtrFactory<ApnMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_APN_MIGRATOR_H_
