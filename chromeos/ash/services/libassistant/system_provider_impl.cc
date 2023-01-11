// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/system_provider_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/services/libassistant/power_manager_provider_impl.h"

namespace ash::libassistant {

SystemProviderImpl::SystemProviderImpl(
    std::unique_ptr<PowerManagerProviderImpl> power_manager_provider)
    : power_manager_provider_(std::move(power_manager_provider)) {}

SystemProviderImpl::~SystemProviderImpl() = default;

void SystemProviderImpl::Initialize(
    mojom::PlatformDelegate* platform_delegate) {
  if (power_manager_provider_)
    power_manager_provider_->Initialize(platform_delegate);

  platform_delegate->BindBatteryMonitor(
      battery_monitor_.BindNewPipeAndPassReceiver());
  battery_monitor_->QueryNextStatus(base::BindOnce(
      &SystemProviderImpl::OnBatteryStatus, base::Unretained(this)));
}

assistant_client::MicMuteState SystemProviderImpl::GetMicMuteState() {
  // CRAS input is never muted.
  return assistant_client::MicMuteState::MICROPHONE_ENABLED;
}

void SystemProviderImpl::RegisterMicMuteChangeCallback(
    ConfigChangeCallback callback) {
  // No need to register since it will never change.
}

assistant_client::PowerManagerProvider*
SystemProviderImpl::GetPowerManagerProvider() {
  return power_manager_provider_.get();
}

bool SystemProviderImpl::GetBatteryState(BatteryState* state) {
  if (!current_battery_status_)
    return false;

  state->is_charging = current_battery_status_->charging;
  state->charge_percentage =
      static_cast<int>(current_battery_status_->level * 100);
  return true;
}

void SystemProviderImpl::UpdateTimezoneAndLocale(const std::string& timezone,
                                                 const std::string& locale) {}

void SystemProviderImpl::OnBatteryStatus(
    device::mojom::BatteryStatusPtr battery_status) {
  current_battery_status_ = std::move(battery_status);

  // Battery monitor is one shot, send another query to get battery status
  // updates. This query will only return when a status changes.
  battery_monitor_->QueryNextStatus(base::BindOnce(
      &SystemProviderImpl::OnBatteryStatus, base::Unretained(this)));
}

void SystemProviderImpl::FlushForTesting() {
  battery_monitor_.FlushForTesting();  // IN-TEST
}

}  // namespace ash::libassistant
