// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"

#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"

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
                                apn->authentication);
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

// static
void CellularNetworkMetricsLogger::LogModifyCustomApnResult(
    bool success,
    std::vector<chromeos::network_config::mojom::ApnType> old_apn_types,
    absl::optional<chromeos::network_config::mojom::ApnState> apn_state,
    absl::optional<chromeos::network_config::mojom::ApnState> old_apn_state) {
  using ApnState = chromeos::network_config::mojom::ApnState;
  base::UmaHistogramBoolean(kModifyCustomApnResultHistogram, success);

  bool has_apn_state = old_apn_state.has_value() && apn_state.has_value();
  bool was_apn_disabled = has_apn_state &&
                          old_apn_state == ApnState::kEnabled &&
                          apn_state == ApnState::kDisabled;
  bool was_apn_enabled = has_apn_state &&
                         old_apn_state == ApnState::kDisabled &&
                         apn_state == ApnState::kEnabled;

  if (was_apn_enabled) {
    base::UmaHistogramBoolean(kEnableCustomApnResultHistogram, success);
  } else if (was_apn_disabled) {
    base::UmaHistogramBoolean(kDisableCustomApnResultHistogram, success);
  }

  // Only emit APN property metrics if the APN was successfully modified.
  if (!success) {
    return;
  }

  absl::optional<CellularNetworkMetricsLogger::ApnTypes> apn_types_enum =
      GetApnTypes(old_apn_types);
  if (!apn_types_enum.has_value()) {
    NET_LOG(DEBUG) << "ApnTypes not logged for APN because it "
                   << "doesn't have any APN types.";
    return;
  }
  base::UmaHistogramEnumeration(kModifyCustomApnApnTypesHistogram,
                                apn_types_enum.value());
  if (was_apn_enabled) {
    base::UmaHistogramEnumeration(kEnableCustomApnApnTypesHistogram,
                                  apn_types_enum.value());
  } else if (was_apn_disabled) {
    base::UmaHistogramEnumeration(kDisableCustomApnApnTypesHistogram,
                                  apn_types_enum.value());
  }
}

// static
void CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
    UnmanagedApnMigrationType type) {
  base::UmaHistogramEnumeration(kCustomApnsUnmanagedMigrationTypeHistogram,
                                type);
}

// static
void CellularNetworkMetricsLogger::LogManagedCustomApnMigrationType(
    ManagedApnMigrationType type) {
  base::UmaHistogramEnumeration(kCustomApnsManagedMigrationTypeHistogram, type);
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

  size_t custom_apns_count = 0u;
  size_t enabled_custom_apns_count = 0u;
  const base::Value::List* custom_apn_list =
      network_metadata_store_->GetCustomApnList(network_state->guid());
  if (custom_apn_list) {
    custom_apns_count = custom_apn_list->size();

    if (ash::features::IsApnRevampEnabled()) {
      enabled_custom_apns_count = std::count_if(
          custom_apn_list->begin(), custom_apn_list->end(),
          [](const base::Value& apn) -> bool {
            const std::string* apn_type =
                apn.GetDict().FindString(::onc::cellular_apn::kState);
            return *apn_type == ::onc::cellular_apn::kStateEnabled;
          });
    }
  }

  // If the connection was successful, log the number of custom APNs the network
  // has saved for it.
  if (!shill_error) {
    base::UmaHistogramCounts100(kCustomApnsCountHistogram, custom_apns_count);

    if (ash::features::IsApnRevampEnabled() && custom_apns_count > 0) {
      base::UmaHistogramCounts100(kCustomApnsEnabledCountHistogram,
                                  enabled_custom_apns_count);
      base::UmaHistogramCounts100(
          kCustomApnsDisabledCountHistogram,
          custom_apns_count - enabled_custom_apns_count);
    }
  }

  // For pre-revamp cases, we consider all custom APNs to be enabled.
  const bool has_enabled_custom_apns = ash::features::IsApnRevampEnabled()
                                           ? (enabled_custom_apns_count > 0)
                                           : (custom_apns_count > 0);
  base::UmaHistogramEnumeration(
      has_enabled_custom_apns ? kConnectResultHasEnabledCustomApnsAllHistogram
                              : kConnectResultNoEnabledCustomApnsAllHistogram,
      connect_result);
}

}  // namespace ash
