// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/scheduler/battery_status_listener_mac.h"

namespace download {

BatteryStatusListenerMac::BatteryStatusListenerMac() = default;

BatteryStatusListenerMac::~BatteryStatusListenerMac() = default;

int BatteryStatusListenerMac::GetBatteryPercentage() {
  return 100;
}

base::PowerStateObserver::BatteryPowerStatus
BatteryStatusListenerMac::GetBatteryPowerStatus() const {
  return base::PowerStateObserver::BatteryPowerStatus::kUnknown;
}

void BatteryStatusListenerMac::Start(Observer* observer) {}

void BatteryStatusListenerMac::Stop() {}

}  // namespace download
