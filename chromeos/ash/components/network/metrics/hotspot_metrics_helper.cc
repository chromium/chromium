// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/hotspot_configuration_handler.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/network_event_log.h"

namespace ash {

// static
const base::TimeDelta HotspotMetricsHelper::kLogAllowStatusAtLoginTimeout =
    base::Seconds(30);

// static
const char HotspotMetricsHelper::kHotspotAllowStatusHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Capability.AllowStatus";

// static
const char HotspotMetricsHelper::kHotspotAllowStatusAtLoginHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Capability.AllowStatusAtLogin";

// static
const char HotspotMetricsHelper::kHotspotEnableResultHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.EnableHotspot.OperationResult";

// static
const char HotspotMetricsHelper::kHotspotDisableResultHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.DisableHotspot.OperationResult";

// static
const char HotspotMetricsHelper::kHotspotSetConfigResultHistogram[] =
    "Network.Ash.Hotspot.SetConfig.OperationResult";

// static
const char HotspotMetricsHelper::kHotspotCheckReadinessResultHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.CheckReadiness.OperationResult";

// static
const char HotspotMetricsHelper::kHotspotUsageConfigAutoDisable[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Usage.Config.AutoDisable";

// static
const char HotspotMetricsHelper::kHotspotUsageConfigMAR[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Usage.Config.MAR";

// static
const char HotspotMetricsHelper::kHotspotUsageConfigCompatibilityMode[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Usage.Config.CompatibilityMode";

// static
const char HotspotMetricsHelper::kHotspotUsageDuration[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Usage.Duration";

// static
const char HotspotMetricsHelper::kHotspotMaxClientCount[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Usage.MaxConnectedDeviceCount";

// static
const char HotspotMetricsHelper::kHotspotIsDeviceManaged[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Usage."
    "ManagedStateWhenHotspotEnabled";

// static
const char HotspotMetricsHelper::kHotspotEnableLatency[] =
    "Network.Ash.Hotspot.Upstream.Cellular.EnableHotspot.Latency";

// static
const char HotspotMetricsHelper::kHotspotUpstreamStatusWhenEnabled[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Enabled.UpstreamStatus";

// static
const char HotspotMetricsHelper::kHotspotDisableReasonHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Disabled.Reason";

// static
void HotspotMetricsHelper::RecordSetTetheringEnabledResult(
    bool enabled,
    hotspot_config::mojom::HotspotControlResult result) {
  HotspotMetricsSetEnabledResult metrics_result =
      GetSetEnabledMetricsResult(result);
  if (enabled) {
    base::UmaHistogramEnumeration(kHotspotEnableResultHistogram,
                                  metrics_result);
    return;
  }
  base::UmaHistogramEnumeration(kHotspotDisableResultHistogram, metrics_result);
}

// static
void HotspotMetricsHelper::RecordCheckTetheringReadinessResult(
    HotspotCapabilitiesProvider::CheckTetheringReadinessResult result) {
  base::UmaHistogramEnumeration(kHotspotCheckReadinessResultHistogram,
                                GetCheckReadinessMetricsResult(result));
}

// static
void HotspotMetricsHelper::RecordSetHotspotConfigResult(
    hotspot_config::mojom::SetHotspotConfigResult result,
    const std::string& shill_error) {
  base::UmaHistogramEnumeration(kHotspotSetConfigResultHistogram,
                                GetSetConfigMetricsResult(result, shill_error));
}

// static
void HotspotMetricsHelper::RecordEnableHotspotLatency(
    const base::TimeDelta& latency) {
  base::UmaHistogramMediumTimes(kHotspotEnableLatency, latency);
}

// static
HotspotMetricsHelper::HotspotMetricsSetEnabledResult
HotspotMetricsHelper::GetSetEnabledMetricsResult(
    const hotspot_config::mojom::HotspotControlResult& result) {
  using hotspot_config::mojom::HotspotControlResult;

  switch (result) {
    case HotspotControlResult::kSuccess:
      return HotspotMetricsSetEnabledResult::kSuccess;
    case HotspotControlResult::kNotAllowed:
      return HotspotMetricsSetEnabledResult::kNotAllowed;
    case HotspotControlResult::kReadinessCheckFailed:
      return HotspotMetricsSetEnabledResult::kReadinessCheckFailure;
    case HotspotControlResult::kDisableWifiFailed:
      return HotspotMetricsSetEnabledResult::kDisableWifiFailure;
    case HotspotControlResult::kInvalidConfiguration:
      return HotspotMetricsSetEnabledResult::kInvalidConfiguration;
    case HotspotControlResult::kUpstreamNotAvailable:
      return HotspotMetricsSetEnabledResult::kUpstreamNotAvailable;
    case HotspotControlResult::kNetworkSetupFailure:
      return HotspotMetricsSetEnabledResult::kNetworkSetupFailure;
    case HotspotControlResult::kDownstreamWifiFailure:
      return HotspotMetricsSetEnabledResult::kDownstreamWifiFailure;
    case HotspotControlResult::kUpstreamFailure:
      return HotspotMetricsSetEnabledResult::kUpstreamFailure;
    case HotspotControlResult::kShillOperationFailed:
      return HotspotMetricsSetEnabledResult::kShillOperationFailure;
    case HotspotControlResult::kAlreadyFulfilled:
      return HotspotMetricsSetEnabledResult::kAlreadyFulfilled;
    case HotspotControlResult::kAborted:
      return HotspotMetricsSetEnabledResult::kAborted;
    case HotspotControlResult::kInvalid:
      return HotspotMetricsSetEnabledResult::kInvalid;
    case HotspotControlResult::kBusy:
      return HotspotMetricsSetEnabledResult::kBusy;
    case HotspotControlResult::kConcurrencyNotSupported:
      return HotspotMetricsSetEnabledResult::kConcurrencyNotSupported;
    case HotspotControlResult::kOperationFailure:
      return HotspotMetricsSetEnabledResult::kOperationFailure;
    default:
      return HotspotMetricsSetEnabledResult::kUnknownFailure;
  }
}

// static
HotspotMetricsHelper::HotspotMetricsCheckReadinessResult
HotspotMetricsHelper::GetCheckReadinessMetricsResult(
    const HotspotCapabilitiesProvider::CheckTetheringReadinessResult& result) {
  using CheckReadinessResult =
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult;

  switch (result) {
    case CheckReadinessResult::kReady:
      return HotspotMetricsCheckReadinessResult::kReady;
    case CheckReadinessResult::kNotAllowedByCarrier:
      return HotspotMetricsCheckReadinessResult::kNotAllowedByCarrier;
    case CheckReadinessResult::kNotAllowedOnFW:
      return HotspotMetricsCheckReadinessResult::kNotAllowedOnFW;
    case CheckReadinessResult::kNotAllowedOnVariant:
      return HotspotMetricsCheckReadinessResult::kNotAllowedOnVariant;
    case CheckReadinessResult::kNotAllowedUserNotEntitled:
      return HotspotMetricsCheckReadinessResult::kNotAllowedUserNotEntitled;
    case CheckReadinessResult::kNotAllowed:
      return HotspotMetricsCheckReadinessResult::kNotAllowed;
    case CheckReadinessResult::kUpstreamNetworkNotAvailable:
      return HotspotMetricsCheckReadinessResult::kUpstreamNetworkNotAvailable;
    case CheckReadinessResult::kShillOperationFailed:
      return HotspotMetricsCheckReadinessResult::kShillOperationFailed;
    case CheckReadinessResult::kUnknownResult:
      return HotspotMetricsCheckReadinessResult::kUnknownResult;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown check tethering readiness result.";
}

// static
HotspotMetricsHelper::HotspotMetricsSetConfigResult
HotspotMetricsHelper::GetSetConfigMetricsResult(
    const hotspot_config::mojom::SetHotspotConfigResult& result,
    const std::string& shill_error) {
  using hotspot_config::mojom::SetHotspotConfigResult;

  switch (result) {
    case SetHotspotConfigResult::kSuccess:
      return HotspotMetricsSetConfigResult::kSuccess;
    case SetHotspotConfigResult::kFailedNotLogin:
      return HotspotMetricsSetConfigResult::kFailedNotLogin;
    case SetHotspotConfigResult::kFailedInvalidConfiguration:
      return HotspotMetricsSetConfigResult::kFailedInvalidConfiguration;
    case SetHotspotConfigResult::kFailedShillOperation:
      if (shill_error == shill::kErrorResultInvalidArguments) {
        return HotspotMetricsSetConfigResult::kFailedInvalidArgument;
      } else if (shill_error == shill::kErrorResultIllegalOperation) {
        return HotspotMetricsSetConfigResult::kFailedIllegalOperation;
      } else if (shill_error == shill::kErrorResultPermissionDenied) {
        return HotspotMetricsSetConfigResult::kFailedPermissionDenied;
      } else if (shill_error == shill::kErrorResultFailure) {
        return HotspotMetricsSetConfigResult::kFailedShillOperation;
      }
      return HotspotMetricsSetConfigResult::kFailedUnknownShillError;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown set hotspot config result.";
}

// static
HotspotMetricsHelper::HotspotMetricsDisableReason
HotspotMetricsHelper::GetMetricsDisableReason(
    const hotspot_config::mojom::DisableReason& reason) {
  using hotspot_config::mojom::DisableReason;

  switch (reason) {
    case DisableReason::kAutoDisabled:
      return HotspotMetricsDisableReason::kAutoDisabled;
    case DisableReason::kInternalError:
      return HotspotMetricsDisableReason::kInternalError;
    case DisableReason::kUserInitiated:
      return HotspotMetricsDisableReason::kUserInitiated;
    case DisableReason::kWifiEnabled:
      return HotspotMetricsDisableReason::kWifiEnabled;
    case DisableReason::kProhibitedByPolicy:
      return HotspotMetricsDisableReason::kProhibitedByPolicy;
    case DisableReason::kUpstreamNetworkNotAvailable:
      return HotspotMetricsDisableReason::kUpstreamNetworkNotAvailable;
    case DisableReason::kSuspended:
      return HotspotMetricsDisableReason::kSuspended;
    case DisableReason::kRestart:
      return HotspotMetricsDisableReason::kRestart;
    case DisableReason::kUpstreamNoInternet:
      return HotspotMetricsDisableReason::kUpstreamNoInternet;
    case DisableReason::kDownstreamLinkDisconnect:
      return HotspotMetricsDisableReason::kDownstreamLinkDisconnect;
    case DisableReason::kDownstreamNetworkDisconnect:
      return HotspotMetricsDisableReason::kDownstreamNetworkDisconnect;
    case DisableReason::kStartTimeout:
      return HotspotMetricsDisableReason::kStartTimeout;
    case DisableReason::kUpstreamNotAvailable:
      return HotspotMetricsDisableReason::kUpstreamNotAvailable;
    case DisableReason::kResourceBusy:
      return HotspotMetricsDisableReason::kResourceBusy;
    case DisableReason::kUnknownError:
      return HotspotMetricsDisableReason::kUnknownError;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown hotspot disable reason.";
}

HotspotMetricsHelper::HotspotMetricsHelper() = default;

HotspotMetricsHelper::~HotspotMetricsHelper() {
  // Log related metrics, namely usage duration and max connected client count
  // if the user logout while hotspot is active.
  if (is_hotspot_active_) {
    LogUsageDuration();
    LogMaxClientCount();
  }
  if (hotspot_capabilities_provider_ &&
      hotspot_capabilities_provider_->HasObserver(this)) {
    hotspot_capabilities_provider_->RemoveObserver(this);
  }
  if (hotspot_state_handler_ && hotspot_state_handler_->HasObserver(this)) {
    hotspot_state_handler_->RemoveObserver(this);
  }

  if (LoginState::IsInitialized()) {
    LoginState::Get()->RemoveObserver(this);
  }
}

void HotspotMetricsHelper::Init(
    EnterpriseManagedMetadataStore* enterprise_managed_metadata_store,
    HotspotCapabilitiesProvider* hotspot_capabilities_provider,
    HotspotStateHandler* hotspot_state_handler,
    HotspotController* hotspot_controller,
    HotspotConfigurationHandler* hotspot_configuration_handler,
    HotspotEnabledStateNotifier* hotspot_enabled_state_notifier,
    NetworkStateHandler* network_state_handler) {
  enterprise_managed_metadata_store_ = enterprise_managed_metadata_store;
  hotspot_state_handler_ = hotspot_state_handler;
  hotspot_state_handler_->AddObserver(this);
  hotspot_capabilities_provider_ = hotspot_capabilities_provider;
  hotspot_capabilities_provider_->AddObserver(this);
  hotspot_configuration_handler_ = hotspot_configuration_handler;
  hotspot_enabled_state_notifier_ = hotspot_enabled_state_notifier;
  network_state_handler_ = network_state_handler;

  if (LoginState::IsInitialized()) {
    LoginState::Get()->AddObserver(this);
    LoggedInStateChanged();
  }

  hotspot_enabled_state_notifier->ObserveEnabledStateChanges(
      hotspot_enabled_state_notifier_receiver_.BindNewPipeAndPassRemote());
}

void HotspotMetricsHelper::OnHotspotCapabilitiesChanged() {
  LogAllowStatus();
}

void HotspotMetricsHelper::LoggedInStateChanged() {
  if (!LoginState::Get()->IsUserLoggedIn()) {
    timer_.Stop();
    is_metrics_logged_ = false;
    return;
  }

  timer_.Start(FROM_HERE, kLogAllowStatusAtLoginTimeout, this,
               &HotspotMetricsHelper::LogAllowStatusAtLogin);
}

void HotspotMetricsHelper::LogAllowStatus() {
  std::optional<HotspotMetricsAllowStatus> metrics_allow_status =
      GetMetricsAllowStatus();
  if (!metrics_allow_status) {
    return;
  }

  base::UmaHistogramEnumeration(kHotspotAllowStatusHistogram,
                                *metrics_allow_status);
}

void HotspotMetricsHelper::LogAllowStatusAtLogin() {
  if (is_metrics_logged_) {
    return;
  }

  std::optional<HotspotMetricsAllowStatus> metrics_allow_status =
      GetMetricsAllowStatus();
  if (!metrics_allow_status) {
    return;
  }

  base::UmaHistogramEnumeration(kHotspotAllowStatusAtLoginHistogram,
                                *metrics_allow_status);
  is_metrics_logged_ = true;
}

std::optional<HotspotMetricsHelper::HotspotMetricsAllowStatus>
HotspotMetricsHelper::GetMetricsAllowStatus() {
  using hotspot_config::mojom::HotspotAllowStatus;

  HotspotAllowStatus allow_status =
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status;

  switch (allow_status) {
    case HotspotAllowStatus::kDisallowedNoWiFiDownstream:
      return HotspotMetricsAllowStatus::kDisallowedWiFiDownstreamNotSupported;
    case HotspotAllowStatus::kDisallowedNoWiFiSecurityModes:
      return HotspotMetricsAllowStatus::kDisallowedNoWiFiSecurityModes;
    case HotspotAllowStatus::kDisallowedNoMobileData:
      return HotspotMetricsAllowStatus::kDisallowedNoMobileData;
    case HotspotAllowStatus::kDisallowedReadinessCheckFail:
      return HotspotMetricsAllowStatus::kDisallowedReadinessCheckFail;
    case HotspotAllowStatus::kDisallowedByPolicy:
      return HotspotMetricsAllowStatus::kDisallowedByPolicy;
    case HotspotAllowStatus::kAllowed:
      return HotspotMetricsAllowStatus::kAllowed;
    case HotspotAllowStatus::kDisallowedNoCellularUpstream:
      // Do not emit kDisallowedNoCellularUpstream which means the device is
      // not cellular capable. Otherwise, it would drown out the metric.
      return std::nullopt;
  }
}

void HotspotMetricsHelper::LogUsageConfig() {
  auto hotspot_config = hotspot_configuration_handler_->GetHotspotConfig();
  if (!hotspot_config) {
    NET_LOG(ERROR) << "Error getting hotspot config when hotspot is turned on.";
    return;
  }
  base::UmaHistogramBoolean(kHotspotUsageConfigAutoDisable,
                            hotspot_config->auto_disable);
  base::UmaHistogramBoolean(
      kHotspotUsageConfigCompatibilityMode,
      hotspot_config->band == hotspot_config::mojom::WiFiBand::k2_4GHz);
  base::UmaHistogramBoolean(kHotspotUsageConfigMAR,
                            hotspot_config->bssid_randomization);
}

void HotspotMetricsHelper::LogUsageDuration() {
  if (!usage_timer_) {
    NET_LOG(ERROR) << "Hotspot usage timer has not been started.";
    return;
  }
  const base::TimeDelta usage_duration = usage_timer_->Elapsed();
  base::UmaHistogramLongTimes(kHotspotUsageDuration, usage_duration);
}

void HotspotMetricsHelper::LogMaxClientCount() {
  base::UmaHistogramCounts100(kHotspotMaxClientCount, max_client_count_);
}

void HotspotMetricsHelper::LogIsDeviceManaged() {
  bool is_enterprise_managed =
      enterprise_managed_metadata_store_->is_enterprise_managed();
  base::UmaHistogramBoolean(kHotspotIsDeviceManaged, is_enterprise_managed);
}

void HotspotMetricsHelper::LogUpstreamStatus() {
  const NetworkState* connected_cellular_network =
      network_state_handler_->ConnectedNetworkByType(
          NetworkTypePattern::Cellular());
  if (!connected_cellular_network) {
    base::UmaHistogramEnumeration(
        kHotspotUpstreamStatusWhenEnabled,
        HotspotMetricsUpstreamStatus::kWifiWithCellularNotConnected);
    return;
  }
  base::UmaHistogramEnumeration(
      kHotspotUpstreamStatusWhenEnabled,
      HotspotMetricsUpstreamStatus::kWifiWithCellularConnected);
}

void HotspotMetricsHelper::LogDisableReason(
    const hotspot_config::mojom::DisableReason& reason) {
  base::UmaHistogramEnumeration(kHotspotDisableReasonHistogram,
                                GetMetricsDisableReason(reason));
}

void HotspotMetricsHelper::OnHotspotTurnedOn() {
  is_hotspot_active_ = true;
  LogUpstreamStatus();
  LogUsageConfig();
  LogIsDeviceManaged();

  usage_timer_ = base::ElapsedTimer();
  max_client_count_ = 0;
}

void HotspotMetricsHelper::OnHotspotTurnedOff(
    hotspot_config::mojom::DisableReason reason) {
  is_hotspot_active_ = false;
  LogDisableReason(reason);
  LogUsageDuration();
  LogMaxClientCount();
  max_client_count_ = 0;
}

void HotspotMetricsHelper::OnHotspotStatusChanged() {
  size_t client_count = hotspot_state_handler_->GetHotspotActiveClientCount();
  max_client_count_ = std::max(max_client_count_, client_count);
}

}  // namespace ash
