// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"

#include "base/metrics/histogram_functions.h"

#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

CellularNetworkMetricsLogger::CellularNetworkMetricsLogger(
    NetworkStateHandler* network_state_handler,
    NetworkMetadataStore* network_metadata_store,
    ConnectionInfoMetricsLogger* connection_info_metrics_logger)
    : network_state_handler_(network_state_handler),
      network_metadata_store_(network_metadata_store) {
  if (connection_info_metrics_logger) {
    connection_info_metrics_logger_observation_.Observe(
        connection_info_metrics_logger);
  }
}

CellularNetworkMetricsLogger::~CellularNetworkMetricsLogger() = default;

void CellularNetworkMetricsLogger::OnConnectionResult(
    const std::string& guid,
    const absl::optional<std::string>& shill_error) {
  DCHECK(network_metadata_store_)
      << "OnConnectionResult() called with no NetworkMetadataStore.";

  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network_state) {
    NET_LOG(ERROR)
        << "OnConnectionResult() call but no network found for guid: " << guid;
    return;
  }

  // Ignore any non-cellular networks.
  if (network_state->GetNetworkTechnologyType() !=
      NetworkState::NetworkTechnologyType::kCellular) {
    return;
  }

  ShillConnectResult connect_result =
      shill_error ? ShillErrorToConnectResult(*shill_error)
                  : ShillConnectResult::kSuccess;

  size_t enabled_custom_apns_count = 0u;
  const base::Value* custom_apn_list =
      network_metadata_store_->GetCustomAPNList(network_state->guid());
  if (custom_apn_list) {
    DCHECK(custom_apn_list->is_list());
    // TODO(b/162365553): Filter on enabled custom APNs when the revamp flag is
    // on.
    enabled_custom_apns_count = custom_apn_list->GetList().size();
  }

  // If the connection was successful, log the number of custom APNs the network
  // has saved for it.
  if (!shill_error) {
    // TODO(b/162365553): Log the number of enabled/disabled APNs.
    base::UmaHistogramCounts100("Network.Ash.Cellular.Apn.CustomApns.Count",
                                enabled_custom_apns_count);
  }

  if (enabled_custom_apns_count > 0) {
    base::UmaHistogramEnumeration(
        "Network.Ash.Cellular.ConnectionResult.HasEnabledCustomApns.All",
        connect_result);
    return;
  }

  base::UmaHistogramEnumeration(
      "Network.Ash.Cellular.ConnectionResult.NoEnabledCustomApns.All",
      connect_result);
}

}  // namespace ash
