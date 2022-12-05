// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_APN_MIGRATOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_APN_MIGRATOR_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class ManagedCellularPrefHandler;
class ManagedNetworkConfigurationHandler;
class NetworkStateHandler;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) ApnMigrator
    : public NetworkStateHandlerObserver {
 public:
  ApnMigrator(
      ManagedCellularPrefHandler* managed_cellular_pref_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkStateHandler* network_state_handler);
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
                                     base::Value::List apn_list);

  // Iterates the registered cellular networks, and clears the Shill property
  // user_apn_list for networks that have been migrated to the APN Revamp
  // feature.
  void ClearUserApnListForMigratedNetworks();

  ManagedCellularPrefHandler* managed_cellular_pref_handler_ = nullptr;
  ManagedNetworkConfigurationHandler* network_configuration_handler_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_APN_MIGRATOR_H_
