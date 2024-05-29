// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"

#include "chromeos/ash/components/dbus/hermes/constants.h"
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

const base::TimeDelta kSmdsScanDurationMinimum = base::Milliseconds(1);
const base::TimeDelta kSmdsScanDurationMaximum = base::Milliseconds(
    ::ash::hermes_constants::kHermesNetworkOperationTimeoutMs);
const size_t kSmdsScanDurationBuckets = 50;

std::optional<CellularNetworkMetricsLogger::ApnTypes> GetApnTypes(
    std::vector<ApnType> apn_types) {
  if (apn_types.empty())
    return std::nullopt;

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

const char* GetESimUserInstallationResultHistogram(
    CellularNetworkMetricsLogger::ESimUserInstallMethod method,
    bool user_errors_included) {
  using ESimUserInstallMethod =
      CellularNetworkMetricsLogger::ESimUserInstallMethod;
  switch (method) {
    case ESimUserInstallMethod::kViaSmds:
      return user_errors_included
                 ? CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsIncludedViaSmds
                 : CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsFilteredViaSmds;
    case ESimUserInstallMethod::kViaActivationCodeAfterSmds:
      return user_errors_included
                 ? CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsIncludedViaActivationCodeAfterSmds
                 : CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsFilteredViaActivationCodeAfterSmds;
    case ESimUserInstallMethod::kViaActivationCodeSkippedSmds:
      return user_errors_included
                 ? CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsIncludedViaActivationCodeSkippedSmds
                 : CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsFilteredViaActivationCodeSkippedSmds;
    case ESimUserInstallMethod::kViaQrCodeAfterSmds:
      return user_errors_included
                 ? CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsIncludedViaQrCodeAfterSmds
                 : CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsFilteredViaQrCodeAfterSmds;
    case ESimUserInstallMethod::kViaQrCodeSkippedSmds:
      return user_errors_included
                 ? CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsIncludedViaQrCodeSkippedSmds
                 : CellularNetworkMetricsLogger::
                       kESimUserInstallUserErrorsFilteredViaQrCodeSkippedSmds;
  }
}

const char* GetESimPolicyInstallationResultHistogram(
    CellularNetworkMetricsLogger::ESimPolicyInstallMethod method,
    bool is_initial,
    bool user_errors_included) {
  using ESimPolicyInstallMethod =
      CellularNetworkMetricsLogger::ESimPolicyInstallMethod;
  switch (method) {
    case ESimPolicyInstallMethod::kViaSmdp:
      if (is_initial) {
        return user_errors_included
                   ? CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsIncludedViaSmdpInitial
                   : CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsFilteredViaSmdpInitial;
      }
      return user_errors_included
                 ? CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsIncludedViaSmdpRetry
                 : CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsFilteredViaSmdpRetry;
    case ESimPolicyInstallMethod::kViaSmds:
      if (is_initial) {
        return user_errors_included
                   ? CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsIncludedViaSmdsInitial
                   : CellularNetworkMetricsLogger::
                         kESimPolicyInstallUserErrorsFilteredViaSmdsInitial;
      }
      return user_errors_included
                 ? CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsIncludedViaSmdsRetry
                 : CellularNetworkMetricsLogger::
                       kESimPolicyInstallUserErrorsFilteredViaSmdsRetry;
  }
}

bool IsAndroidActivationCode(const std::string& smds_activation_code) {
  return smds_activation_code == cellular_utils::kSmdsAndroidProduction ||
         smds_activation_code == cellular_utils::kSmdsAndroidStaging;
}

bool IsGmsaActivationCode(const std::string& smds_activation_code) {
  return smds_activation_code == cellular_utils::kSmdsGsma;
}

const char* GetSmdsScanResultHistogram(const std::string& smds_activation_code,
                                       bool user_errors_included) {
  if (IsAndroidActivationCode(smds_activation_code)) {
    return user_errors_included ? CellularNetworkMetricsLogger::
                                      kESimSmdsScanAndroidUserErrorsIncluded
                                : CellularNetworkMetricsLogger::
                                      kESimSmdsScanAndroidUserErrorsFiltered;
  }
  if (smds_activation_code == cellular_utils::kSmdsGsma) {
    return user_errors_included ? CellularNetworkMetricsLogger::
                                      kESimSmdsScanGsmaUserErrorsIncluded
                                : CellularNetworkMetricsLogger::
                                      kESimSmdsScanGsmaUserErrorsFiltered;
  }
  return user_errors_included ? CellularNetworkMetricsLogger::
                                    kESimSmdsScanOtherUserErrorsIncluded
                              : CellularNetworkMetricsLogger::
                                    kESimSmdsScanOtherUserErrorsFiltered;
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

  std::optional<CellularNetworkMetricsLogger::ApnTypes> apn_types =
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
void CellularNetworkMetricsLogger::LogCreateExclusivelyEnabledCustomApnResult(
    bool success,
    chromeos::network_config::mojom::ApnPropertiesPtr apn) {
  base::UmaHistogramBoolean(kCreateExclusivelyEnabledCustomApnResultHistogram,
                            success);

  // Only emit APN property metrics if the APN was successfully added.
  if (!success) {
    return;
  }

  base::UmaHistogramEnumeration(
      kCreateExclusivelyEnabledCustomApnAuthenticationTypeHistogram,
      apn->authentication);
  base::UmaHistogramEnumeration(
      kCreateExclusivelyEnabledCustomApnIpTypeHistogram, apn->ip_type);

  std::optional<CellularNetworkMetricsLogger::ApnTypes> apn_types =
      GetApnTypes(apn->apn_types);
  if (!apn_types.has_value()) {
    NET_LOG(DEBUG) << "CreateExclusivelyEnabledCustomApn.ApnTypes not logged "
                   << "for APN because it doesn't have any APN types.";
    return;
  }
  base::UmaHistogramEnumeration(
      kCreateExclusivelyEnabledCustomApnApnTypesHistogram, apn_types.value());
}

// static
void CellularNetworkMetricsLogger::LogRemoveCustomApnResult(
    bool success,
    std::vector<chromeos::network_config::mojom::ApnType> apn_types) {
  base::UmaHistogramBoolean(kRemoveCustomApnResultHistogram, success);

  // Only emit APN property metrics if the APN was successfully removed.
  if (!success)
    return;

  std::optional<CellularNetworkMetricsLogger::ApnTypes> apn_types_enum =
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
    std::optional<chromeos::network_config::mojom::ApnState> apn_state,
    std::optional<chromeos::network_config::mojom::ApnState> old_apn_state) {
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

  std::optional<CellularNetworkMetricsLogger::ApnTypes> apn_types_enum =
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
void CellularNetworkMetricsLogger::LogSmdsScanProfileCount(
    size_t count,
    SmdsScanMethod method) {
  switch (method) {
    case SmdsScanMethod::kViaPolicy:
      base::UmaHistogramCounts100(kSmdsScanViaPolicyProfileCount, count);
      break;
    case SmdsScanMethod::kViaUser:
      base::UmaHistogramCounts100(kSmdsScanViaUserProfileCount, count);
      break;
  }
}

// static
void CellularNetworkMetricsLogger::LogSmdsScanDuration(
    const base::TimeDelta& duration,
    bool success,
    const std::string& smds_activation_code) {
  std::string histogram;
  if (IsAndroidActivationCode(smds_activation_code)) {
    histogram = success ? kSmdsScanAndroidDurationSuccess
                        : kSmdsScanAndroidDurationFailure;
  } else if (IsGmsaActivationCode(smds_activation_code)) {
    histogram =
        success ? kSmdsScanGsmaDurationSuccess : kSmdsScanGsmaDurationFailure;
  } else {
    histogram =
        success ? kSmdsScanOtherDurationSuccess : kSmdsScanOtherDurationFailure;
  }
  base::UmaHistogramCustomTimes(histogram, duration, kSmdsScanDurationMinimum,
                                kSmdsScanDurationMaximum,
                                kSmdsScanDurationBuckets);
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

// static
void CellularNetworkMetricsLogger::LogESimPolicyInstallNoAvailableProfiles(
    ESimPolicyInstallMethod method) {
  base::UmaHistogramEnumeration(kESimPolicyInstallNoAvailableProfiles, method);
}

// static
void CellularNetworkMetricsLogger::LogESimUserInstallResult(
    ESimUserInstallMethod method,
    ESimOperationResult result,
    bool is_user_error) {
  if (!is_user_error) {
    base::UmaHistogramEnumeration(kESimUserInstallUserErrorsFilteredAll,
                                  result);
    base::UmaHistogramEnumeration(GetESimUserInstallationResultHistogram(
                                      method, /*user_errors_included=*/false),
                                  result);
  }
  base::UmaHistogramEnumeration(kESimUserInstallUserErrorsIncludedAll, result);
  base::UmaHistogramEnumeration(GetESimUserInstallationResultHistogram(
                                    method, /*user_errors_included=*/true),
                                result);
}

// static
void CellularNetworkMetricsLogger::LogESimPolicyInstallResult(
    ESimPolicyInstallMethod method,
    ESimOperationResult result,
    bool is_initial,
    bool is_user_error) {
  if (!is_user_error) {
    base::UmaHistogramEnumeration(kESimPolicyInstallUserErrorsFilteredAll,
                                  result);
    base::UmaHistogramEnumeration(GetESimPolicyInstallationResultHistogram(
                                      method, /*is_initial=*/is_initial,
                                      /*user_errors_included=*/false),
                                  result);
  }
  base::UmaHistogramEnumeration(kESimPolicyInstallUserErrorsIncludedAll,
                                result);
  base::UmaHistogramEnumeration(
      GetESimPolicyInstallationResultHistogram(
          method, /*is_initial=*/is_initial, /*user_errors_included=*/true),
      result);
}

// static
void CellularNetworkMetricsLogger::LogSmdsScanResult(
    const std::string& smds_activation_code,
    std::optional<HermesResponseStatus> status) {
  const bool is_user_error =
      status.has_value() &&
      CellularNetworkMetricsLogger::HermesResponseStatusIsUserError(*status);
  const ESimOperationResult result =
      CellularNetworkMetricsLogger::ComputeESimOperationResult(status);

  if (!is_user_error) {
    base::UmaHistogramEnumeration(
        GetSmdsScanResultHistogram(smds_activation_code,
                                   /*user_errors_included=*/false),
        result);
  }
  base::UmaHistogramEnumeration(
      GetSmdsScanResultHistogram(smds_activation_code,
                                 /*user_errors_included=*/true),
      result);
}

// static
CellularNetworkMetricsLogger::ESimOperationResult
CellularNetworkMetricsLogger::ComputeESimOperationResult(
    std::optional<HermesResponseStatus> status) {
  if (status.has_value()) {
    return *status == HermesResponseStatus::kSuccess
               ? ESimOperationResult::kSuccess
               : ESimOperationResult::kHermesFailed;
  }
  return ESimOperationResult::kInhibitFailed;
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
    case HermesResponseStatus::kErrorNoResponse:
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
void CellularNetworkMetricsLogger::LogUserTextMessageSuppressionState(
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
  base::UmaHistogramEnumeration(kUserAllowTextMessagesSuppressionStateHistogram,
                                histogram_type);
}

// static
void CellularNetworkMetricsLogger::LogPolicyTextMessageSuppressionState(
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
      kPolicyAllowTextMessagesSuppressionStateHistogram, histogram_type);
}

// static
void CellularNetworkMetricsLogger::CellularNetworkMetricsLogger::
    LogTextMessageNotificationSuppressionState(
        NotificationSuppressionState state) {
  base::UmaHistogramEnumeration(kAllowTextMessagesNotificationSuppressionState,
                                state);
}

void CellularNetworkMetricsLogger::OnConnectionResult(
    const std::string& guid,
    const std::optional<std::string>& shill_error) {
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
