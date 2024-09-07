// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/esim_policy_login_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

namespace {

// Checks whether the current logged in user type is an owner or regular.
bool IsLoggedInUserRegular() {
  if (!LoginState::IsInitialized())
    return false;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  return user_type == LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR;
}

void LogBlockNonManagedCellularHistogram(bool allow_only_managed_cellular) {
  UMA_HISTOGRAM_ENUMERATION(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      allow_only_managed_cellular
          ? ESimPolicyLoginMetricsLogger::BlockNonManagedCellularBehavior::
                kAllowManagedOnly
          : ESimPolicyLoginMetricsLogger::BlockNonManagedCellularBehavior::
                kAllowUnmanaged);
}

}  // namespace

// static
const base::TimeDelta ESimPolicyLoginMetricsLogger::kInitializationTimeout =
    base::Seconds(15);

// static
const char ESimPolicyLoginMetricsLogger::
    kESimPolicyBlockNonManagedCellularHistogram[] =
        "Network.Cellular.ESim.Policy.BlockNonManagedCellularBehavior";

// static
const char ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram[] =
    "Network.Cellular.ESim.Policy.StatusAtLogin";

// static
bool ESimPolicyLoginMetricsLogger::last_allow_only_managed_cellular_ = false;

// static
void ESimPolicyLoginMetricsLogger::RecordBlockNonManagedCellularBehavior(
    bool allow_only_managed_cellular) {
  if (last_allow_only_managed_cellular_ == allow_only_managed_cellular)
    return;
  last_allow_only_managed_cellular_ = allow_only_managed_cellular;
  LogBlockNonManagedCellularHistogram(allow_only_managed_cellular);
}

ESimPolicyLoginMetricsLogger::ESimPolicyLoginMetricsLogger() = default;

ESimPolicyLoginMetricsLogger::~ESimPolicyLoginMetricsLogger() {
  if (initialized_ && LoginState::IsInitialized()) {
    LoginState::Get()->RemoveObserver(this);
  }
}

void ESimPolicyLoginMetricsLogger::Init(
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  network_state_handler_ = network_state_handler;
  managed_network_configuration_handler_ =
      managed_network_configuration_handler;
  network_state_handler_observer_.Observe(network_state_handler_.get());

  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  // Devices may already be present before this method is called.
  // Make sure that lists and timers are initialized properly.
  DeviceListChanged();
  initialized_ = true;
}

void ESimPolicyLoginMetricsLogger::LoggedInStateChanged() {
  if (!IsLoggedInUserRegular()) {
    return;
  }

  is_metrics_logged_ = false;
  if (is_cellular_available_)
    LogESimPolicyStatusAtLogin();
}

void ESimPolicyLoginMetricsLogger::LogESimPolicyStatusAtLogin() {
  NetworkStateHandler::DeviceStateList device_list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &device_list);
  // Only collect the metrics on cellular capable device.
  is_cellular_available_ = !device_list.empty();
  if (!is_cellular_available_ || is_metrics_logged_ ||
      !IsLoggedInUserRegular() || !is_enterprise_managed_) {
    return;
  }

  LogBlockNonManagedCellularHistogram(managed_network_configuration_handler_
                                          ->AllowOnlyPolicyCellularNetworks());

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);

  bool has_managed_cellular = false;
  bool has_non_managed_cellular = false;
  for (const auto* network : network_list) {
    if (network->IsManagedByPolicy()) {
      has_managed_cellular = true;
    } else {
      has_non_managed_cellular = true;
    }
  }

  const ESimPolicyStatusAtLogin result = GetESimPolicyStatusAtLogin(
      has_managed_cellular, has_non_managed_cellular);
  UMA_HISTOGRAM_ENUMERATION(kESimPolicyStatusAtLoginHistogram, result);
  is_metrics_logged_ = true;
}

void ESimPolicyLoginMetricsLogger::DeviceListChanged() {
  NetworkStateHandler::DeviceStateList device_list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &device_list);
  if (device_list.empty())
    return;

  // Start a timer to wait for cellular networks to initialize.
  // This makes sure that intermediate not-connected states are
  // not logged before initialization is completed.
  initialization_timer_.Start(
      FROM_HERE, kInitializationTimeout, this,
      &ESimPolicyLoginMetricsLogger::LogESimPolicyStatusAtLogin);
}

void ESimPolicyLoginMetricsLogger::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void ESimPolicyLoginMetricsLogger::SetIsEnterpriseManaged(
    bool is_enterprise_managed) {
  is_enterprise_managed_ = is_enterprise_managed;
  LogESimPolicyStatusAtLogin();
}

ESimPolicyLoginMetricsLogger::ESimPolicyStatusAtLogin
ESimPolicyLoginMetricsLogger::GetESimPolicyStatusAtLogin(
    bool has_managed_cellular,
    bool has_non_managed_cellular) {
  if (has_managed_cellular && has_non_managed_cellular) {
    return ESimPolicyStatusAtLogin::kManagedAndUnmanaged;
  }

  if (has_managed_cellular) {
    return ESimPolicyStatusAtLogin::kManagedOnly;
  }

  if (has_non_managed_cellular) {
    return ESimPolicyStatusAtLogin::kUnmanagedOnly;
  }

  return ESimPolicyStatusAtLogin::kNoCellularNetworks;
}

}  // namespace ash
