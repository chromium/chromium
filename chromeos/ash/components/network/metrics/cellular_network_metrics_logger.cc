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
namespace {

using ApnType = chromeos::network_config::mojom::ApnType;

absl::optional<CellularNetworkMetricsLogger::ApnTypes> GetApnTypes(
    std::vector<ApnType> apn_types) {
  if (apn_types.empty())
    return absl::nullopt;

  bool is_default = false;
  bool is_attach = false;
  for (const auto& apn_type : apn_types) {
    if (apn_type == ApnType::kDefault)
      is_default = true;
    if (apn_type == ApnType::kAttach)
      is_attach = true;
  }
  if (is_default && is_attach)
    return CellularNetworkMetricsLogger::ApnTypes::kDefaultAndAttach;

  if (is_attach)
    return CellularNetworkMetricsLogger::ApnTypes::kAttach;

  return CellularNetworkMetricsLogger::ApnTypes::kDefault;
}

}  // namespace

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

// static
void CellularNetworkMetricsLogger::LogCreateCustomApnResult(
    bool success,
    chromeos::network_config::mojom::ApnPropertiesPtr apn) {
  base::UmaHistogramBoolean(kCreateCustomApnResultHistogram, success);

  // Only emit APN property metrics if the APN was successfully added.
  if (!success)
    return;

  base::UmaHistogramEnumeration(kCreateCustomApnAuthenticationTypeHistogram,
                                apn->authentication_type);
  base::UmaHistogramEnumeration(kCreateCustomApnIpTypeHistogram, apn->ip_type);

  absl::optional<CellularNetworkMetricsLogger::ApnTypes> apn_types =
      GetApnTypes(apn->apn_types);
  if (!apn_types.has_value()) {
    NET_LOG(DEBUG) << "CreateCustomApn.ApnTypes not logged for APN because it "
                   << "doesn't have any APN types.";
    return;
  }
  base::UmaHistogramEnumeration(kCreateCustomApnApnTypesHistogram,
                                apn_types.value());
}

// static
void CellularNetworkMetricsLogger::LogRemoveCustomApnResult(
    bool success,
    std::vector<chromeos::network_config::mojom::ApnType> apn_types) {
  base::UmaHistogramBoolean(kRemoveCustomApnResultHistogram, success);

  // Only emit APN property metrics if the APN was successfully removed.
  if (!success)
    return;

  absl::optional<CellularNetworkMetricsLogger::ApnTypes> apn_types_enum =
      GetApnTypes(apn_types);
  if (!apn_types_enum.has_value()) {
    NET_LOG(DEBUG) << "RemoveCustomApn.ApnTypes not logged for APN because it "
                   << "doesn't have any APN types.";
    return;
  }
  base::UmaHistogramEnumeration(kRemoveCustomApnApnTypesHistogram,
                                apn_types_enum.value());
}

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
  const base::Value::List* custom_apn_list =
      network_metadata_store_->GetCustomApnList(network_state->guid());
  if (custom_apn_list) {
    // TODO(b/162365553): Filter on enabled custom APNs when the revamp flag is
    // on.
    enabled_custom_apns_count = custom_apn_list->size();
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
