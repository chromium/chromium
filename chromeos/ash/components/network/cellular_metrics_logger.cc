// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_metrics_logger.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/tick_clock.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kESimUMAFeatureName[] = "ESim";

// Checks whether the current logged in user type is an owner or regular.
bool IsLoggedInUserOwnerOrRegular() {
  if (!LoginState::IsInitialized())
    return false;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  return user_type == LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER ||
         user_type == LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR;
}

SimType GetSimType(const NetworkState* network) {
  return network->eid().empty() ? SimType::kPSim : SimType::kESim;
}

}  // namespace

// static
const char
    CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram[] =
        "Network.Cellular.ESim.ConnectionResult.UserInitiated";

// static
const char
    CellularMetricsLogger::kPSimUserInitiatedConnectionResultHistogram[] =
        "Network.Cellular.PSim.ConnectionResult.UserInitiated";

// static
const char CellularMetricsLogger::kESimAllConnectionResultHistogram[] =
    "Network.Cellular.ESim.ConnectionResult.All";

// static
const char CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram[] =
    "Network.Cellular.ESim.Policy.ConnectionResult.All";

// static
const char CellularMetricsLogger::kPSimAllConnectionResultHistogram[] =
    "Network.Cellular.PSim.ConnectionResult.All";

// static
const char CellularMetricsLogger::kSimPinRequireLockSuccessHistogram[] =
    "Network.Cellular.Pin.RequireLockSuccess";

// static
const char CellularMetricsLogger::kSimPinRemoveLockSuccessHistogram[] =
    "Network.Cellular.Pin.RemoveLockSuccess";

// static
const char CellularMetricsLogger::kUnmanagedSimPinUnlockSuccessHistogram[] =
    "Network.Cellular.Pin.Unmanaged.UnlockSuccess";

// static
const char CellularMetricsLogger::kManagedSimPinUnlockSuccessHistogram[] =
    "Network.Cellular.Pin.Managed.UnlockSuccess";

// static
const char CellularMetricsLogger::kUnmanagedSimPinUnblockSuccessHistogram[] =
    "Network.Cellular.Pin.Unmanaged.UnblockSuccess";

// static
const char CellularMetricsLogger::kManagedSimPinUnblockSuccessHistogram[] =
    "Network.Cellular.Pin.Managed.UnblockSuccess";

// static
const char CellularMetricsLogger::kChangePinSuccessSimPinLockPolicyHistogram[] =
    "Network.Cellular.ChangePin.SimPINLockPolicy";

// static
const char
    CellularMetricsLogger::kRequirePinSuccessSimPinLockPolicyHistogram[] =
        "Network.Cellular.RequirePin.SimPINLockPolicy";

// static
const char CellularMetricsLogger::kSimPinChangeSuccessHistogram[] =
    "Network.Cellular.Pin.ChangeSuccess";

// static
const char CellularMetricsLogger::kSimLockNotificationEventHistogram[] =
    "Network.Ash.Cellular.SimLock.Policy.Notification.Event";

// static
const char CellularMetricsLogger::kUnrestrictedSimPinUnlockSuccessHistogram[] =
    "Network.Cellular.Pin.Unrestricted.UnlockSuccess";

// static
const char CellularMetricsLogger::kRestrictedSimPinUnlockSuccessHistogram[] =
    "Network.Cellular.Pin.Restricted.UnlockSuccess";

// static
const char CellularMetricsLogger::kUnrestrictedSimPinUnblockSuccessHistogram[] =
    "Network.Cellular.Pin.Unrestricted.UnblockSuccess";

// static
const char CellularMetricsLogger::kRestrictedSimPinUnblockSuccessHistogram[] =
    "Network.Cellular.Pin.Restricted.UnblockSuccess";

// static
const char CellularMetricsLogger::kSimLockNotificationLockType[] =
    "Network.Ash.Cellular.SimLock.Policy.Notification.LockType";

// static
const char CellularMetricsLogger::kUnrestrictedActiveNetworkSIMLockStatus[] =
    "Network.Ash.Cellular.SimLock.Policy.Unrestricted.ActiveSIMLockStatus";

// static
const char CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus[] =
    "Network.Ash.Cellular.SimLock.Policy.Restricted.ActiveSIMLockStatus";

// static
const base::TimeDelta CellularMetricsLogger::kInitializationTimeout =
    base::Seconds(15);

// static
const base::TimeDelta CellularMetricsLogger::kDisconnectRequestTimeout =
    base::Seconds(5);

// static
CellularMetricsLogger::SimPinOperationResult
CellularMetricsLogger::GetSimPinOperationResultForShillError(
    const std::string& shill_error_name) {
  if (shill_error_name == shill::kErrorResultFailure ||
      shill_error_name == shill::kErrorResultInvalidArguments) {
    return SimPinOperationResult::kErrorFailure;
  }
  if (shill_error_name == shill::kErrorResultNotSupported)
    return SimPinOperationResult::kErrorNotSupported;
  if (shill_error_name == shill::kErrorResultIncorrectPin)
    return SimPinOperationResult::kErrorIncorrectPin;
  if (shill_error_name == shill::kErrorResultPinBlocked)
    return SimPinOperationResult::kErrorPinBlocked;
  if (shill_error_name == shill::kErrorResultPinRequired)
    return SimPinOperationResult::kErrorPinRequired;
  if (shill_error_name == shill::kErrorResultNotFound)
    return SimPinOperationResult::kErrorDeviceMissing;
  if (shill_error_name == shill::kErrorResultWrongState)
    return SimPinOperationResult::kErrorWrongState;
  return SimPinOperationResult::kErrorUnknown;
}

// static
void CellularMetricsLogger::RecordSimLockNotificationEvent(
    const SimLockNotificationEvent notification_event) {
  base::UmaHistogramEnumeration(kSimLockNotificationEventHistogram,
                                notification_event);
}

// static
void CellularMetricsLogger::RecordSimLockNotificationLockType(
    const std::string& sim_lock_type) {
  if (sim_lock_type == shill::kSIMLockPin) {
    base::UmaHistogramEnumeration(kSimLockNotificationLockType,
                                  SimPinLockType::kPinLocked);
  } else if (sim_lock_type == shill::kSIMLockPuk) {
    base::UmaHistogramEnumeration(kSimLockNotificationLockType,
                                  SimPinLockType::kPukLocked);
  } else {
    NOTREACHED();
  }
}

// static
void CellularMetricsLogger::RecordSimPinOperationResult(
    const SimPinOperation& pin_operation,
    const bool allow_cellular_sim_lock,
    const absl::optional<std::string>& shill_error_name) {
  SimPinOperationResult result =
      shill_error_name.has_value()
          ? GetSimPinOperationResultForShillError(*shill_error_name)
          : SimPinOperationResult::kSuccess;
  bool is_enterprise_managed = NetworkHandler::Get()->is_enterprise_managed();

  switch (pin_operation) {
    case SimPinOperation::kRequireLock:
      base::UmaHistogramBoolean(kRequirePinSuccessSimPinLockPolicyHistogram,
                                allow_cellular_sim_lock);
      base::UmaHistogramEnumeration(kSimPinRequireLockSuccessHistogram, result);
      return;
    case SimPinOperation::kRemoveLock:
      base::UmaHistogramEnumeration(kSimPinRemoveLockSuccessHistogram, result);
      return;
    case SimPinOperation::kUnlock:
      if (is_enterprise_managed) {
        base::UmaHistogramEnumeration(kManagedSimPinUnlockSuccessHistogram,
                                      result);
        base::UmaHistogramEnumeration(
            allow_cellular_sim_lock ? kUnrestrictedSimPinUnlockSuccessHistogram
                                    : kRestrictedSimPinUnlockSuccessHistogram,
            result);
      } else {
        base::UmaHistogramEnumeration(kUnmanagedSimPinUnlockSuccessHistogram,
                                      result);
      }
      return;
    case SimPinOperation::kUnblock:
      if (is_enterprise_managed) {
        base::UmaHistogramEnumeration(kManagedSimPinUnblockSuccessHistogram,
                                      result);
        base::UmaHistogramEnumeration(
            allow_cellular_sim_lock ? kUnrestrictedSimPinUnblockSuccessHistogram
                                    : kRestrictedSimPinUnblockSuccessHistogram,
            result);
      } else {
        base::UmaHistogramEnumeration(kUnmanagedSimPinUnblockSuccessHistogram,
                                      result);
      }
      return;
    case SimPinOperation::kChange:
      base::UmaHistogramBoolean(kChangePinSuccessSimPinLockPolicyHistogram,
                                allow_cellular_sim_lock);
      base::UmaHistogramEnumeration(kSimPinChangeSuccessHistogram, result);
      return;
  }
}

// static
void CellularMetricsLogger::LogCellularUserInitiatedConnectionSuccessHistogram(
    CellularMetricsLogger::ConnectResult start_connect_result,
    SimType sim_type) {
  if (sim_type == SimType::kPSim) {
    base::UmaHistogramEnumeration(kPSimUserInitiatedConnectionResultHistogram,
                                  start_connect_result);
  } else {
    base::UmaHistogramEnumeration(kESimUserInitiatedConnectionResultHistogram,
                                  start_connect_result);
  }
}

// static
CellularMetricsLogger::ConnectResult
CellularMetricsLogger::NetworkConnectionErrorToConnectResult(
    const std::string& error_name) {
  if (error_name == NetworkConnectionHandler::kErrorNotFound)
    return CellularMetricsLogger::ConnectResult::kInvalidGuid;

  if (error_name == NetworkConnectionHandler::kErrorConnected ||
      error_name == NetworkConnectionHandler::kErrorConnecting ||
      error_name == NetworkConnectionHandler::kErrorNotConnected ||
      error_name ==
          NetworkConnectionHandler::kErrorTetherAttemptWithNoDelegate) {
    return CellularMetricsLogger::ConnectResult::kInvalidState;
  }

  if (error_name == NetworkConnectionHandler::kErrorConnectCanceled)
    return CellularMetricsLogger::ConnectResult::kCanceled;

  if (error_name == NetworkConnectionHandler::kErrorPassphraseRequired ||
      error_name == NetworkConnectionHandler::kErrorBadPassphrase ||
      error_name == NetworkConnectionHandler::kErrorCertificateRequired ||
      error_name == NetworkConnectionHandler::kErrorConfigurationRequired ||
      error_name == NetworkConnectionHandler::kErrorAuthenticationRequired ||
      error_name == NetworkConnectionHandler::kErrorCertLoadTimeout ||
      error_name == NetworkConnectionHandler::kErrorConfigureFailed ||
      error_name == NetworkConnectionHandler::kErrorHexSsidRequired) {
    return CellularMetricsLogger::ConnectResult::kNotConfigured;
  }

  if (error_name == NetworkConnectionHandler::kErrorBlockedByPolicy)
    return CellularMetricsLogger::ConnectResult::kBlocked;

  if (error_name == NetworkConnectionHandler::kErrorCellularInhibitFailure)
    return CellularMetricsLogger::ConnectResult::kCellularInhibitFailure;

  if (error_name == NetworkConnectionHandler::kErrorESimProfileIssue)
    return CellularMetricsLogger::ConnectResult::kESimProfileIssue;

  if (error_name == NetworkConnectionHandler::kErrorCellularOutOfCredits)
    return CellularMetricsLogger::ConnectResult::kCellularOutOfCredits;

  if (error_name == NetworkConnectionHandler::kErrorSimLocked)
    return CellularMetricsLogger::ConnectResult::kSimLocked;

  if (error_name == NetworkConnectionHandler::kErrorConnectFailed)
    return CellularMetricsLogger::ConnectResult::kConnectFailed;

  if (error_name == NetworkConnectionHandler::kErrorActivateFailed)
    return CellularMetricsLogger::ConnectResult::kActivateFailed;

  if (error_name ==
      NetworkConnectionHandler::kErrorEnabledOrDisabledWhenNotAvailable) {
    return CellularMetricsLogger::ConnectResult::
        kEnabledOrDisabledWhenNotAvailable;
  }

  if (error_name == NetworkConnectionHandler::kErrorCellularDeviceBusy)
    return CellularMetricsLogger::ConnectResult::kErrorCellularDeviceBusy;

  if (error_name == NetworkConnectionHandler::kErrorConnectTimeout)
    return CellularMetricsLogger::ConnectResult::kErrorConnectTimeout;

  if (error_name == NetworkConnectionHandler::kConnectableCellularTimeout)
    return CellularMetricsLogger::ConnectResult::kConnectableCellularTimeout;

  return CellularMetricsLogger::ConnectResult::kUnknown;
}

// static
CellularMetricsLogger::ShillConnectResult
CellularMetricsLogger::ShillErrorToConnectResult(
    const std::string& error_name) {
  if (error_name == shill::kErrorBadPassphrase)
    return CellularMetricsLogger::ShillConnectResult::kBadPassphrase;
  else if (error_name == shill::kErrorBadWEPKey)
    return CellularMetricsLogger::ShillConnectResult::kBadWepKey;
  else if (error_name == shill::kErrorConnectFailed)
    return CellularMetricsLogger::ShillConnectResult::kFailedToConnect;
  else if (error_name == shill::kErrorDhcpFailed)
    return CellularMetricsLogger::ShillConnectResult::kDhcpFailure;
  else if (error_name == shill::kErrorDNSLookupFailed)
    return CellularMetricsLogger::ShillConnectResult::kDnsLookupFailure;
  else if (error_name == shill::kErrorEapAuthenticationFailed)
    return CellularMetricsLogger::ShillConnectResult::kEapAuthentication;
  else if (error_name == shill::kErrorEapLocalTlsFailed)
    return CellularMetricsLogger::ShillConnectResult::kEapLocalTls;
  else if (error_name == shill::kErrorEapRemoteTlsFailed)
    return CellularMetricsLogger::ShillConnectResult::kEapRemoteTls;
  else if (error_name == shill::kErrorOutOfRange)
    return CellularMetricsLogger::ShillConnectResult::kOutOfRange;
  else if (error_name == shill::kErrorPinMissing)
    return CellularMetricsLogger::ShillConnectResult::kPinMissing;
  else if (error_name == shill::kErrorNoFailure)
    return CellularMetricsLogger::ShillConnectResult::kNoFailure;
  else if (error_name == shill::kErrorNotAssociated)
    return CellularMetricsLogger::ShillConnectResult::kNotAssociated;
  else if (error_name == shill::kErrorNotAuthenticated)
    return CellularMetricsLogger::ShillConnectResult::kNotAuthenticated;
  else if (error_name == shill::kErrorSimLocked)
    return CellularMetricsLogger::ShillConnectResult::kErrorSimLocked;
  else if (error_name == shill::kErrorNotRegistered)
    return CellularMetricsLogger::ShillConnectResult::kErrorNotRegistered;
  return CellularMetricsLogger::ShillConnectResult::kUnknown;
}

// Reports daily ESim Standard Feature Usage Logging metrics. Note that
// if an object of this type is destroyed and created in the same day,
// metrics eligibility and enablement will only be reported once. Registers
// to local state prefs instead of profile prefs as cellular network is
// available to anyone using the device, as opposed to per profile basis.
class ESimFeatureUsageMetrics
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  explicit ESimFeatureUsageMetrics(
      CellularESimProfileHandler* cellular_esim_profile_handler)
      : cellular_esim_profile_handler_(cellular_esim_profile_handler) {
    DCHECK(cellular_esim_profile_handler);
    feature_usage_metrics_ =
        std::make_unique<feature_usage::FeatureUsageMetrics>(
            kESimUMAFeatureName, this);
  }

  ~ESimFeatureUsageMetrics() override = default;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const final {
    // If the device is eligible to use ESim.
    return HermesManagerClient::Get()->GetAvailableEuiccs().size() != 0;
  }

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEnabled() const final {
    // If there are installed ESim profiles.
    for (const auto& profile :
         cellular_esim_profile_handler_->GetESimProfiles()) {
      if (profile.state() == CellularESimProfile::State::kActive ||
          profile.state() == CellularESimProfile::State::kInactive) {
        return true;
      }
    }
    return false;
  }

  // Should be called after an attempt to connect to an ESim profile.
  void RecordUsage(bool success) const {
    feature_usage_metrics_->RecordUsage(success);
  }

  void StartUsage() { feature_usage_metrics_->StartSuccessfulUsage(); }
  void StopUsage() { feature_usage_metrics_->StopSuccessfulUsage(); }

 private:
  CellularESimProfileHandler* cellular_esim_profile_handler_;
  std::unique_ptr<feature_usage::FeatureUsageMetrics> feature_usage_metrics_;
};

void CellularMetricsLogger::LogCellularAllConnectionSuccessHistogram(
    CellularMetricsLogger::ShillConnectResult start_connect_result,
    SimType sim_type,
    bool is_managed_by_policy) {
  if (sim_type == SimType::kPSim) {
    base::UmaHistogramEnumeration(kPSimAllConnectionResultHistogram,
                                  start_connect_result);
  } else {
    base::UmaHistogramEnumeration(kESimAllConnectionResultHistogram,
                                  start_connect_result);
    if (is_managed_by_policy) {
      base::UmaHistogramEnumeration(kESimPolicyAllConnectionResultHistogram,
                                    start_connect_result);
    }

    // If there is a failure to connect, log a failed usage attempt to
    // FeatureUsageMetrics.
    if (start_connect_result !=
        CellularMetricsLogger::ShillConnectResult::kSuccess) {
      esim_feature_usage_metrics_->RecordUsage(/*success=*/false);
    }
  }
}

CellularMetricsLogger::ConnectionInfo::ConnectionInfo(
    const std::string& network_guid,
    bool is_connected,
    bool is_connecting)
    : network_guid(network_guid),
      is_connected(is_connected),
      is_connecting(is_connecting) {}

CellularMetricsLogger::ConnectionInfo::ConnectionInfo(
    const std::string& network_guid)
    : network_guid(network_guid) {}

CellularMetricsLogger::ConnectionInfo::~ConnectionInfo() = default;

CellularMetricsLogger::CellularMetricsLogger() = default;

CellularMetricsLogger::~CellularMetricsLogger() {
  if (network_state_handler_)
    OnShuttingDown();

  if (initialized_) {
    if (LoginState::IsInitialized())
      LoginState::Get()->RemoveObserver(this);

    if (network_connection_handler_)
      network_connection_handler_->RemoveObserver(this);
  }
}

void CellularMetricsLogger::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  network_state_handler_ = network_state_handler;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  managed_network_configuration_handler_ =
      managed_network_configuration_handler;
  network_state_handler_observer_.Observe(network_state_handler_);

  if (network_connection_handler) {
    network_connection_handler_ = network_connection_handler;
    network_connection_handler_->AddObserver(this);
  }

  if (cellular_esim_profile_handler_) {
    esim_feature_usage_metrics_ = std::make_unique<ESimFeatureUsageMetrics>(
        cellular_esim_profile_handler_);
  }

  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  // Devices and networks may already be present before this method is called.
  // Make sure that lists and timers are initialized properly.
  DeviceListChanged();
  NetworkListChanged();
  initialized_ = true;
}

void CellularMetricsLogger::DeviceListChanged() {
  NetworkStateHandler::DeviceStateList device_list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &device_list);
  bool new_is_cellular_available = !device_list.empty();
  if (is_cellular_available_ == new_is_cellular_available)
    return;

  is_cellular_available_ = new_is_cellular_available;
  // Start a timer to wait for cellular networks to initialize.
  // This makes sure that intermediate not-connected states are
  // not logged before initialization is completed.
  if (is_cellular_available_) {
    initialization_timer_.Start(
        FROM_HERE, kInitializationTimeout, this,
        &CellularMetricsLogger::OnInitializationTimeout);
  }
}

void CellularMetricsLogger::NetworkListChanged() {
  base::flat_map<std::string, std::unique_ptr<ConnectionInfo>>
      old_connection_info_map;
  // Clear |guid_to_connection_info_map| so that only new and existing
  // networks are added back to it.
  old_connection_info_map.swap(guid_to_connection_info_map_);

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);

  // Check the current cellular networks list and copy existing connection info
  // from old map to new map or create new ones if it does not exist.
  for (const auto* network : network_list) {
    const std::string& guid = network->guid();
    auto old_connection_info_map_iter = old_connection_info_map.find(guid);
    if (old_connection_info_map_iter != old_connection_info_map.end()) {
      guid_to_connection_info_map_.insert_or_assign(
          guid, std::move(old_connection_info_map_iter->second));
      old_connection_info_map.erase(old_connection_info_map_iter);
      continue;
    }

    guid_to_connection_info_map_.insert_or_assign(
        guid,
        std::make_unique<ConnectionInfo>(guid, network->IsConnectedState(),
                                         network->IsConnectingState()));
  }
}

void CellularMetricsLogger::OnInitializationTimeout() {
  CheckForPSimActivationStateMetric();
  CheckForESimProfileStatusMetric();
  CheckForCellularUsageMetrics();
  CheckForCellularServiceCountMetric();
}

void CellularMetricsLogger::LoggedInStateChanged() {
  if (!IsLoggedInUserOwnerOrRegular())
    return;

  // This flag enures that activation state is only logged once when
  // the user logs in.
  is_psim_activation_state_logged_ = false;
  CheckForPSimActivationStateMetric();

  // This flag enures that activation state is only logged once when
  // the user logs in.
  is_esim_profile_status_logged_ = false;
  CheckForESimProfileStatusMetric();

  // This flag ensures that the service count is only logged once when
  // the user logs in.
  is_service_count_logged_ = false;
  CheckForCellularServiceCountMetric();
}

void CellularMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  DCHECK(network_state_handler_);
  CheckForCellularUsageMetrics();

  if (network->type().empty() ||
      !network->Matches(NetworkTypePattern::Cellular())) {
    return;
  }

  CheckForTimeToConnectedMetric(network);
  // Check for connection failures triggered by shill changes, unlike in
  // ConnectFailed() which is triggered by connection attempt failures at
  // chrome layers.
  CheckForShillConnectionFailureMetric(network);
  CheckForConnectionStateMetric(network);
  CheckForSIMStatusMetric(network);
}

void CellularMetricsLogger::CheckForSIMStatusMetric(
    const NetworkState* network) {
  const DeviceState* cellular_device =
      network_state_handler_->GetDeviceState(network->device_path());
  if (!cellular_device || network->IsConnectingState()) {
    return;
  }

  const std::string& sim_lock_type = cellular_device->sim_lock_type();

  if (last_active_network_iccid_ == network->iccid() ||
      (!network->IsConnectedState() && sim_lock_type.empty())) {
    return;
  }

  last_active_network_iccid_ = network->iccid();
  SimPinLockType lock_type;

  if (sim_lock_type == shill::kSIMLockPin) {
    lock_type = SimPinLockType::kPinLocked;
  } else if (sim_lock_type == shill::kSIMLockPuk) {
    lock_type = SimPinLockType::kPukLocked;
  } else if (sim_lock_type.empty()) {
    lock_type = SimPinLockType::kUnlocked;
  } else {
    NOTREACHED();
  }

  if (managed_network_configuration_handler_->AllowCellularSimLock()) {
    base::UmaHistogramEnumeration(kUnrestrictedActiveNetworkSIMLockStatus,
                                  lock_type);
  } else {
    base::UmaHistogramEnumeration(kRestrictedActiveNetworkSIMLockStatus,
                                  lock_type);
  }
}

void CellularMetricsLogger::CheckForTimeToConnectedMetric(
    const NetworkState* network) {
  if (network->activation_state() != shill::kActivationStateActivated)
    return;

  // We could be receiving a connection state change for a network different
  // from the one observed when the start time was recorded. Make sure that we
  // only look up time to connected of the corresponding network.
  ConnectionInfo* connection_info =
      GetConnectionInfoForCellularNetwork(network->guid());

  if (network->IsConnectingState()) {
    if (!connection_info->last_connect_start_time.has_value())
      connection_info->last_connect_start_time = base::TimeTicks::Now();

    return;
  }

  if (!connection_info->last_connect_start_time.has_value())
    return;

  if (network->IsConnectedState()) {
    base::TimeDelta time_to_connected =
        base::TimeTicks::Now() - *connection_info->last_connect_start_time;

    if (GetSimType(network) == SimType::kPSim) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Network.Cellular.PSim.TimeToConnected",
                                 time_to_connected);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("Network.Cellular.ESim.TimeToConnected",
                                 time_to_connected);
    }
  }

  // This is hit when the network is no longer in connecting state,
  // successfully connected or otherwise. Reset the connect start_time
  // so that it is not used for further connection state changes.
  connection_info->last_connect_start_time.reset();
}

void CellularMetricsLogger::ConnectSucceeded(const std::string& service_path) {
  const NetworkState* network = GetCellularNetwork(service_path);
  if (!network)
    return;

  LogCellularUserInitiatedConnectionSuccessHistogram(ConnectResult::kSuccess,
                                                     GetSimType(network));
}

void CellularMetricsLogger::ConnectFailed(const std::string& service_path,
                                          const std::string& error_name) {
  const NetworkState* network = GetCellularNetwork(service_path);
  if (!network)
    return;

  LogCellularUserInitiatedConnectionSuccessHistogram(
      NetworkConnectionErrorToConnectResult(error_name), GetSimType(network));
}

void CellularMetricsLogger::DisconnectRequested(
    const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network->Matches(NetworkTypePattern::Cellular()))
    return;

  ConnectionInfo* connection_info =
      GetConnectionInfoForCellularNetwork(network->guid());

  // A disconnect request could fail and result in no cellular connection state
  // change. Save the request time so that only disconnections that do not
  // correspond to a request received within |kDisconnectRequestTimeout| are
  // tracked.
  connection_info->last_disconnect_request_time = base::TimeTicks::Now();
  connection_info->disconnect_requested = true;
}

const NetworkState* CellularMetricsLogger::GetCellularNetwork(
    const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network || !network->Matches(NetworkTypePattern::Cellular()))
    return nullptr;
  return network;
}

CellularMetricsLogger::PSimActivationState
CellularMetricsLogger::PSimActivationStateToEnum(const std::string& state) {
  if (state == shill::kActivationStateActivated)
    return PSimActivationState::kActivated;
  else if (state == shill::kActivationStateActivating)
    return PSimActivationState::kActivating;
  else if (state == shill::kActivationStateNotActivated)
    return PSimActivationState::kNotActivated;
  else if (state == shill::kActivationStatePartiallyActivated)
    return PSimActivationState::kPartiallyActivated;

  return PSimActivationState::kUnknown;
}

void CellularMetricsLogger::LogCellularDisconnectionsHistogram(
    ConnectionState connection_state,
    SimType sim_type,
    bool is_managed_by_policy) {
  if (sim_type == SimType::kPSim) {
    UMA_HISTOGRAM_ENUMERATION("Network.Cellular.PSim.Disconnections",
                              connection_state);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Network.Cellular.ESim.Disconnections",
                              connection_state);
    if (is_managed_by_policy) {
      UMA_HISTOGRAM_ENUMERATION("Network.Cellular.ESim.Policy.Disconnections",
                                connection_state);
    }
  }
}

void CellularMetricsLogger::CheckForShillConnectionFailureMetric(
    const NetworkState* network) {
  ConnectionInfo* connection_info =
      GetConnectionInfoForCellularNetwork(network->guid());

  // If the network connection failed to connect from a connecting state, and no
  // disconnection was requested. Note that |network->connection_state()| being
  // shill::kStateFailure or an empty |network-GetError()| is unreliable after
  // repeated attempts to connect to a network that will fail.
  if (!network->IsConnectingOrConnected() && connection_info->is_connecting &&
      !connection_info->disconnect_requested) {
    LogCellularAllConnectionSuccessHistogram(
        ShillErrorToConnectResult(network->GetError()), GetSimType(network),
        network->IsManagedByPolicy());
  }

  connection_info->is_connecting = network->IsConnectingState();
  connection_info->disconnect_requested = false;
}

void CellularMetricsLogger::CheckForConnectionStateMetric(
    const NetworkState* network) {
  ConnectionInfo* connection_info =
      GetConnectionInfoForCellularNetwork(network->guid());

  bool new_is_connected = network->IsConnectedState();
  if (connection_info->is_connected == new_is_connected)
    return;
  absl::optional<bool> old_is_connected = connection_info->is_connected;
  connection_info->is_connected = new_is_connected;

  if (new_is_connected) {
    LogCellularAllConnectionSuccessHistogram(
        CellularMetricsLogger::ShillConnectResult::kSuccess,
        GetSimType(network), network->IsManagedByPolicy());
    LogCellularDisconnectionsHistogram(ConnectionState::kConnected,
                                       GetSimType(network),
                                       network->IsManagedByPolicy());
    connection_info->last_disconnect_request_time.reset();
    return;
  }

  // If the previous connection state is nullopt then this is a new connection
  // info entry and a disconnection did not really occur. Skip logging the
  // metric in this case.
  if (!old_is_connected.has_value())
    return;

  absl::optional<base::TimeDelta> time_since_disconnect_requested;
  if (connection_info->last_disconnect_request_time) {
    time_since_disconnect_requested =
        base::TimeTicks::Now() - *connection_info->last_disconnect_request_time;
  }

  // If the disconnect occurred in less than |kDisconnectRequestTimeout|
  // from the last disconnect request time then treat it as a user
  // initiated disconnect and skip histogram log.
  if (time_since_disconnect_requested &&
      time_since_disconnect_requested < kDisconnectRequestTimeout) {
    return;
  }
  LogCellularDisconnectionsHistogram(ConnectionState::kDisconnected,
                                     GetSimType(network),
                                     network->IsManagedByPolicy());
}

void CellularMetricsLogger::CheckForESimProfileStatusMetric() {
  if (!cellular_esim_profile_handler_ || !is_cellular_available_ ||
      is_esim_profile_status_logged_ || !IsLoggedInUserOwnerOrRegular()) {
    return;
  }

  std::vector<CellularESimProfile> esim_profiles =
      cellular_esim_profile_handler_->GetESimProfiles();

  bool pending_profiles_exist = false;
  bool active_profiles_exist = false;
  for (const auto& profile : esim_profiles) {
    switch (profile.state()) {
      case CellularESimProfile::State::kPending:
        [[fallthrough]];
      case CellularESimProfile::State::kInstalling:
        pending_profiles_exist = true;
        break;

      case CellularESimProfile::State::kInactive:
        [[fallthrough]];
      case CellularESimProfile::State::kActive:
        active_profiles_exist = true;
        break;
    }
  }

  ESimProfileStatus activation_state;
  if (active_profiles_exist && !pending_profiles_exist)
    activation_state = ESimProfileStatus::kActive;
  else if (active_profiles_exist && pending_profiles_exist)
    activation_state = ESimProfileStatus::kActiveWithPendingProfiles;
  else if (!active_profiles_exist && pending_profiles_exist)
    activation_state = ESimProfileStatus::kPendingProfilesOnly;
  else
    activation_state = ESimProfileStatus::kNoProfiles;

  UMA_HISTOGRAM_ENUMERATION("Network.Cellular.ESim.StatusAtLogin",
                            activation_state);
  is_esim_profile_status_logged_ = true;
}

void CellularMetricsLogger::CheckForPSimActivationStateMetric() {
  if (!is_cellular_available_ || is_psim_activation_state_logged_ ||
      !IsLoggedInUserOwnerOrRegular())
    return;

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);

  if (network_list.size() == 0)
    return;

  absl::optional<std::string> psim_activation_state;
  for (const auto* network : network_list) {
    if (GetSimType(network) == SimType::kPSim)
      psim_activation_state = network->activation_state();
  }

  // No PSim networks exist.
  if (!psim_activation_state.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Network.Cellular.PSim.StatusAtLogin",
                            PSimActivationStateToEnum(*psim_activation_state));
  is_psim_activation_state_logged_ = true;
}

void CellularMetricsLogger::CheckForCellularServiceCountMetric() {
  if (!is_cellular_available_ || is_service_count_logged_ ||
      !IsLoggedInUserOwnerOrRegular()) {
    return;
  }

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);

  size_t psim_networks = 0;
  size_t esim_profiles = 0;
  size_t esim_policy_profiles = 0;

  for (const auto* network : network_list) {
    SimType sim_type = GetSimType(network);
    if (sim_type == SimType::kPSim) {
      psim_networks++;
    } else {
      esim_profiles++;
      if (network->IsManagedByPolicy())
        esim_policy_profiles++;
    }
  }

  if (managed_network_configuration_handler_->AllowCellularSimLock()) {
    UMA_HISTOGRAM_COUNTS_100(
        "Network.Cellular.Unrestricted.PSim.ServiceAtLogin.Count",
        psim_networks);
    UMA_HISTOGRAM_COUNTS_100(
        "Network.Cellular.Unrestricted.ESim.ServiceAtLogin.Count",
        esim_profiles);
  } else {
    UMA_HISTOGRAM_COUNTS_100(
        "Network.Cellular.Restricted.PSim.ServiceAtLogin.Count", psim_networks);
    UMA_HISTOGRAM_COUNTS_100(
        "Network.Cellular.Restricted.ESim.ServiceAtLogin.Count", esim_profiles);
  }

  UMA_HISTOGRAM_COUNTS_100("Network.Cellular.ESim.Policy.ServiceAtLogin.Count",
                           esim_policy_profiles);
  is_service_count_logged_ = true;
}

void CellularMetricsLogger::CheckForCellularUsageMetrics() {
  if (!is_cellular_available_)
    return;

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::NonVirtual(), &network_list);

  absl::optional<const NetworkState*> connected_cellular_network;
  bool is_non_cellular_connected = false;
  for (auto* network : network_list) {
    if (!network->IsConnectedState())
      continue;

    // Note: Only one cellular network may be ever connected.
    if (network->Matches(NetworkTypePattern::Cellular()))
      connected_cellular_network = network;
    else
      is_non_cellular_connected = true;
  }

  // Discard not-connected states received before the timer runs out.
  if (!connected_cellular_network.has_value() &&
      initialization_timer_.IsRunning()) {
    return;
  }

  CellularUsage usage;
  absl::optional<SimType> sim_type;
  bool is_managed_by_policy = false;
  if (connected_cellular_network.has_value()) {
    usage = is_non_cellular_connected
                ? CellularUsage::kConnectedWithOtherNetwork
                : CellularUsage::kConnectedAndOnlyNetwork;
    sim_type = GetSimType(connected_cellular_network.value());
    is_managed_by_policy =
        connected_cellular_network.value()->IsManagedByPolicy();
  } else {
    usage = CellularUsage::kNotConnected;
  }

  if (!sim_type.has_value() || *sim_type == SimType::kPSim) {
    if (usage != last_psim_cellular_usage_) {
      UMA_HISTOGRAM_ENUMERATION("Network.Cellular.PSim.Usage.Count", usage);
      if (last_psim_cellular_usage_ ==
          CellularUsage::kConnectedAndOnlyNetwork) {
        UMA_HISTOGRAM_LONG_TIMES("Network.Cellular.PSim.Usage.Duration",
                                 psim_usage_elapsed_timer_->Elapsed());
      }
    }

    psim_usage_elapsed_timer_ = base::ElapsedTimer();
    last_psim_cellular_usage_ = usage;
  }

  if (!sim_type.has_value() || *sim_type == SimType::kESim) {
    if (usage != last_esim_cellular_usage_) {
      UMA_HISTOGRAM_ENUMERATION("Network.Cellular.ESim.Usage.Count", usage);
      // Logs to ESim.Policy.Usage.Count histogram when the current connected
      // network is managed by policy or previous connected managed cellular
      // network gets disconnected.
      if (is_managed_by_policy ||
          (last_managed_by_policy_ && usage == CellularUsage::kNotConnected)) {
        UMA_HISTOGRAM_ENUMERATION("Network.Cellular.ESim.Policy.Usage.Count",
                                  usage);
      }

      if (last_esim_cellular_usage_ ==
          CellularUsage::kConnectedAndOnlyNetwork) {
        const base::TimeDelta usage_duration =
            esim_usage_elapsed_timer_->Elapsed();

        UMA_HISTOGRAM_LONG_TIMES("Network.Cellular.ESim.Usage.Duration",
                                 usage_duration);
        if (last_managed_by_policy_) {
          UMA_HISTOGRAM_LONG_TIMES(
              "Network.Cellular.ESim.Policy.Usage.Duration", usage_duration);
        }
      }
    }

    HandleESimFeatureUsageChange(
        last_esim_cellular_usage_.value_or(CellularUsage::kNotConnected),
        usage);

    esim_usage_elapsed_timer_ = base::ElapsedTimer();
    last_esim_cellular_usage_ = usage;
    last_managed_by_policy_ = is_managed_by_policy;
  }
}

void CellularMetricsLogger::HandleESimFeatureUsageChange(
    CellularUsage last_usage,
    CellularUsage current_usage) {
  if (!esim_feature_usage_metrics_ || last_usage == current_usage)
    return;

  // If the user first connects to an ESim cellular network, regardless if
  // another network type is connected, record a successful usage. Note that the
  // preference order is Ethernet > Wifi > Cellular. Also note that
  // RecordUsage() should only be called when the usage state transitions from a
  // not connected state (kNotConnected) to a connected state
  // (kConnectedAndOnlyNetwork or kConnectedWithOtherNetwork). I.e RecordUsage()
  // should not be called when the usage state transitions from
  // kConnectedAndOnlyNetwork to kConnectedWithOtherNetwork, and vice versa.
  if (last_usage == CellularUsage::kNotConnected)
    esim_feature_usage_metrics_->RecordUsage(/*success=*/true);

  // If the user is actively using the ESim cellular network, start recording
  // usage time.
  if (current_usage == CellularUsage::kConnectedAndOnlyNetwork)
    esim_feature_usage_metrics_->StartUsage();

  // If the user is no longer actively using the ESim cellular network, stop
  // recording usage time.
  if (last_usage == CellularUsage::kConnectedAndOnlyNetwork)
    esim_feature_usage_metrics_->StopUsage();
}

CellularMetricsLogger::ConnectionInfo*
CellularMetricsLogger::GetConnectionInfoForCellularNetwork(
    const std::string& cellular_network_guid) {
  auto it = guid_to_connection_info_map_.find(cellular_network_guid);

  ConnectionInfo* connection_info;
  if (it == guid_to_connection_info_map_.end()) {
    // We could get connection events in some cases before network
    // list change event. Insert new network into the list.
    auto insert_result = guid_to_connection_info_map_.insert_or_assign(
        cellular_network_guid,
        std::make_unique<ConnectionInfo>(cellular_network_guid));
    connection_info = insert_result.first->second.get();
  } else {
    connection_info = it->second.get();
  }

  return connection_info;
}

void CellularMetricsLogger::OnShuttingDown() {
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
  esim_feature_usage_metrics_.reset();
}

}  // namespace ash
