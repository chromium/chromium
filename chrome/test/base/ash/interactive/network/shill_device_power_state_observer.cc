// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/network/shill_device_power_state_observer.h"

#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

ShillDevicePowerStateObserver::ShillDevicePowerStateObserver(
    ShillManagerClient* manager_client,
    const NetworkTypePattern& network_type_pattern)
    : ObservationStateObserver(manager_client),
      network_type_pattern_(network_type_pattern) {
  if (!NetworkHandler::IsInitialized()) {
    return;
  }
  device_enabled_state_ = IsDeviceEnabled();
}

ShillDevicePowerStateObserver::~ShillDevicePowerStateObserver() = default;

void ShillDevicePowerStateObserver::OnPropertyChanged(
    const std::string& key,
    const base::Value& value) {
  if (key != shill::kEnabledTechnologiesProperty) {
    return;
  }

  const bool device_enabled_state = IsDeviceEnabled();
  if (device_enabled_state == device_enabled_state_) {
    return;
  }
  device_enabled_state_ = device_enabled_state;

  OnStateObserverStateChanged(/*state=*/device_enabled_state_);
}

bool ShillDevicePowerStateObserver::GetStateObserverInitialState() const {
  return device_enabled_state_;
}

bool ShillDevicePowerStateObserver::IsDeviceEnabled() const {
  return NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
      network_type_pattern_);
}

}  // namespace ash
