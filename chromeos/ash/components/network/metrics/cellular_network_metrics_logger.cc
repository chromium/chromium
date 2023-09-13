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
#include "chromeos/ash/components/network/text_message_suppression_state.h"
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

const char* GetESimPolicyInstallationResultHistogram(
    CellularNetworkMetricsLogger::ESimPolicyInstallMethod method,
    bool is_initial,
    bool filtered_variant) {
  using ESimPolicyInstallMethod =
      CellularNetworkMetricsLogger::ESimPolicyInstallMethod;
  switch (method) {
    case ESimPolicyInstallMethod::kViaSmdp:
      if (is_initial) {
        return filtered_variant
                   ? CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsIncludedViaSmdpInitial
                   : CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsFilteredViaSmdpInitial;
      }
      return filtered_variant
                 ? CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsIncludedViaSmdpRetry
                 : CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsFilteredViaSmdpRetry;
    case ESimPolicyInstallMethod::kViaSmds:
      if (is_initial) {
        return filtered_variant
                   ? CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsIncludedViaSmdsInitial
                   : CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsFilteredViaSmdsInitial;
      }
      return filtered_variant
                 ? CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsIncludedViaSmdsRetry
                 : CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsFilteredViaSmdsRetry;
  }
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

// static
void CellularNetworkMetricsLogger::LogSmdsScanProfileCount(size_t count) {
  base::UmaHistogramCounts100(kSmdsScanProfileCount, count);
}

// static
void CellularNetworkMetricsLogger::LogESimUserInstallMethod(
    ESimUserInstallMethod method) {
  base::UmaHistogramEnumeration(kESimUserInstallMethod, method);
}

// static
void CellularNetworkMetricsLogger::LogESimPolicyInstallMethod(
    ESimPolicyInstallMethod method) {
  base::UmaHistogramEnumeration(kESimPolicyInstallMethod, method);
}

void CellularNetworkMetricsLogger::LogESimPolicyInstallResult(
    ESimPolicyInstallMethod method,
    ESimInstallResult result,
    bool is_initial,
    bool is_user_error) {
  if (!is_user_error) {
    base::UmaHistogramEnumeration(kESimPolicyInstallUserErrorsFilteredAll,
                                  result);
    base::UmaHistogramEnumeration(GetESimPolicyInstallationResultHistogram(
                                      method, /*is_initial=*/is_initial,
                                      /*filtered_variant=*/false),
                                  result);
  }
  base::UmaHistogramEnumeration(kESimPolicyInstallUserErrorsIncludedAll,
                                result);
  base::UmaHistogramEnumeration(
      GetESimPolicyInstallationResultHistogram(
          method, /*is_initial=*/is_initial, /*filtered_variant=*/true),
      result);
  GetESimPolicyInstallationResultHistogram(method, is_initial, is_user_error);
}

// static
CellularNetworkMetricsLogger::ESimInstallResult
CellularNetworkMetricsLogger::ComputeESimInstallResult(
    absl::optional<HermesResponseStatus> status) {
  if (status.has_value()) {
    return *status == HermesResponseStatus::kSuccess
               ? ESimInstallResult::kSuccess
               : ESimInstallResult::kHermesFailed;
  }
  return ESimInstallResult::kInhibitFailed;
}

// static
bool CellularNetworkMetricsLogger::HermesResponseStatusIsUserError(
    HermesResponseStatus status) {
  switch (status) {
    case HermesResponseStatus::kSuccess:
      [[fallthrough]];
    case HermesResponseStatus::kErrorUnknown:
      [[fallthrough]];
    case HermesResponseStatus::kErrorInternalLpaFailure:
      [[fallthrough]];
    case HermesResponseStatus::kErrorSendNotificationFailure:
      [[fallthrough]];
    case HermesResponseStatus::kErrorTestProfileInProd:
      [[fallthrough]];
    case HermesResponseStatus::kErrorUnsupported:
      [[fallthrough]];
    case HermesResponseStatus::kErrorWrongState:
      [[fallthrough]];
    case HermesResponseStatus::kErrorBadRequest:
      [[fallthrough]];
    case HermesResponseStatus::kErrorBadNotification:
      [[fallthrough]];
    case HermesResponseStatus::kErrorPendingProfile:
      [[fallthrough]];
    case HermesResponseStatus::kErrorSendApduFailure:
      [[fallthrough]];
    case HermesResponseStatus::kErrorUnexpectedModemManagerState:
      [[fallthrough]];
    case HermesResponseStatus::kErrorModemMessageProcessing:
      [[fallthrough]];
    case HermesResponseStatus::kErrorUnknownResponse:
      return false;
    case HermesResponseStatus::kErrorAlreadyDisabled:
      [[fallthrough]];
    case HermesResponseStatus::kErrorAlreadyEnabled:
      [[fallthrough]];
    case HermesResponseStatus::kErrorInvalidActivationCode:
      [[fallthrough]];
    case HermesResponseStatus::kErrorInvalidIccid:
      [[fallthrough]];
    case HermesResponseStatus::kErrorInvalidParameter:
      [[fallthrough]];
    case HermesResponseStatus::kErrorNeedConfirmationCode:
      [[fallthrough]];
    case HermesResponseStatus::kErrorInvalidResponse:
      [[fallthrough]];
    case HermesResponseStatus::kErrorNoResponse:
      [[fallthrough]];
    case HermesResponseStatus::kErrorMalformedResponse:
      [[fallthrough]];
    case HermesResponseStatus::kErrorSendHttpsFailure:
      [[fallthrough]];
    case HermesResponseStatus::kErrorEmptyResponse:
      return true;
  }
  // Do not provide a default return here; all cases should be handled inside
  // the switch statement above.
}

// static
void CellularNetworkMetricsLogger::LogUserTextMessageSuppressionType(
    ash::UserTextMessageSuppressionState state) {
  UserTextMessageSuppressionState histogram_type;
  switch (state) {
    case ash::UserTextMessageSuppressionState::kAllow:
      histogram_type = UserTextMessageSuppressionState::kTextMessagesAllow;
      break;
    case ash::UserTextMessageSuppressionState::kSuppress:
      histogram_type = UserTextMessageSuppressionState::kTextMessagesSuppress;
      break;
  }
  base::UmaHistogramEnumeration(kUserAllowTextMessagesSuppressionTypeHistogram,
                                histogram_type);
}

// static
void CellularNetworkMetricsLogger::LogPolicyTextMessageSuppressionType(
    ash::PolicyTextMessageSuppressionState state) {
  PolicyTextMessageSuppressionState histogram_type;
  switch (state) {
    case ash::PolicyTextMessageSuppressionState::kAllow:
      histogram_type = PolicyTextMessageSuppressionState::kTextMessagesAllow;
      break;
    case ash::PolicyTextMessageSuppressionState::kSuppress:
      histogram_type = PolicyTextMessageSuppressionState::kTextMessagesSuppress;
      break;
    case ash::PolicyTextMessageSuppressionState::kUnset:
      histogram_type = PolicyTextMessageSuppressionState::kUnset;
      break;
  }
  base::UmaHistogramEnumeration(
      kPolicyAllowTextMessagesSuppressionTypeHistogram, histogram_type);
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
