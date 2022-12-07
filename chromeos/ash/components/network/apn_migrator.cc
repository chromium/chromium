// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/apn_migrator.h"

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

namespace {
void OnSetShillUserApnListSuccess() {}

void OnSetShillUserApnListFailure(const std::string& guid,
                                  const std::string& error_name) {
  NET_LOG(ERROR) << "ApnMigrator: Failed to update the user APN "
                    "list in Shill for network: "
                 << guid << ": [" << error_name << ']';
}
}  // namespace

ApnMigrator::ApnMigrator(
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    ManagedNetworkConfigurationHandler* network_configuration_handler,
    NetworkStateHandler* network_state_handler)
    : managed_cellular_pref_handler_(managed_cellular_pref_handler),
      network_configuration_handler_(network_configuration_handler),
      network_state_handler_(network_state_handler) {
  if (!NetworkHandler::IsInitialized())
    return;
  network_state_handler_observer_.Observe(network_state_handler_);
}

ApnMigrator::~ApnMigrator() = default;

void ApnMigrator::SetShillUserApnListForNetwork(
    const NetworkState& network,
    const base::Value::List* apn_list) {
  network_configuration_handler_->SetProperties(
      network.path(),
      base::Value(
          chromeos::network_config::UserApnListToOnc(network.guid(), apn_list)),
      base::BindOnce(&OnSetShillUserApnListSuccess),
      base::BindOnce(&OnSetShillUserApnListFailure, network.guid()));
}

void ApnMigrator::ClearUserApnListForMigratedNetworks() {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);
  for (const NetworkState* network : network_list) {
    if (managed_cellular_pref_handler_->ContainsApnMigratedIccid(
            network->iccid())) {
      // Clear UserApnList so that Shill knows to use legacy APN selection
      // logic.
      base::Value::List empty_user_apn_list;
      SetShillUserApnListForNetwork(*network, /*apn_list=*/nullptr);
    }
  }
}

void ApnMigrator::NetworkListChanged() {
  if (ash::features::IsApnRevampEnabled()) {
    // TODO(b/162365553): Implement this case: APN revamp enabled
    return;
  }
  ClearUserApnListForMigratedNetworks();
}

}  // namespace ash
