// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/scheduler/battery_status_listener_impl.h"

#include "components/download/internal/background_service/scheduler/device_status.h"

namespace download {

BatteryStatusListenerImpl::BatteryStatusListenerImpl(
    const base::TimeDelta& battery_query_interval)
    : battery_percentage_(100),
      battery_query_interval_(battery_query_interval),
      last_battery_query_(base::Time::Now()),
      observer_(nullptr) {}

BatteryStatusListenerImpl::~BatteryStatusListenerImpl() = default;

int BatteryStatusListenerImpl::GetBatteryPercentage() {
  UpdateBatteryPercentage(false);
  return battery_percentage_;
}

base::PowerStateObserver::BatteryPowerStatus
BatteryStatusListenerImpl::GetBatteryPowerStatus() const {
  return base::PowerMonitor::GetInstance()->GetBatteryPowerStatus();
}

void BatteryStatusListenerImpl::Start(Observer* observer) {
  observer_ = observer;

  auto* power_monitor = base::PowerMonitor::GetInstance();
  DCHECK(power_monitor->IsInitialized());
  power_monitor->AddPowerStateObserver(this);

  UpdateBatteryPercentage(true);
}

void BatteryStatusListenerImpl::Stop() {
  base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
}

int BatteryStatusListenerImpl::GetBatteryPercentageInternal() {
  // Non-Android implementation currently always return full battery.
  return 100;
}

void BatteryStatusListenerImpl::UpdateBatteryPercentage(bool force) {
  // Throttle the battery queries.
  if (!force &&
      base::Time::Now() - last_battery_query_ < battery_query_interval_) {
    return;
  }

  battery_percentage_ = GetBatteryPercentageInternal();
  last_battery_query_ = base::Time::Now();
}

void BatteryStatusListenerImpl::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  if (observer_)
    observer_->OnPowerStateChange(battery_power_status);
}

}  // namespace download
