// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/system_settings_provider.h"

#include <memory>
#include <string_view>

#include "ash/constants/ash_switches.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace ash {

SystemSettingsProvider::SystemSettingsProvider()
    : CrosSettingsProvider(CrosSettingsProvider::NotifyObserversCallback()) {
  Init();
}

SystemSettingsProvider::SystemSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  Init();
}

SystemSettingsProvider::~SystemSettingsProvider() = default;

void SystemSettingsProvider::Init() {
  system::TimezoneSettings* timezone_settings =
      system::TimezoneSettings::GetInstance();
  timezone_settings_observation_.Observe(timezone_settings);
  timezone_value_ =
      std::make_unique<base::Value>(timezone_settings->GetCurrentTimezoneID());
  per_user_timezone_enabled_value_ =
      std::make_unique<base::Value>(switches::IsPerUserTimezoneEnabled());
  fine_grained_time_zone_enabled_value_ = std::make_unique<base::Value>(
      switches::IsFineGrainedTimeZoneDetectionEnabled());
}

const base::Value* SystemSettingsProvider::Get(std::string_view path) const {
  if (path == kSystemTimezone)
    return timezone_value_.get();

  if (path == kPerUserTimezoneEnabled)
    return per_user_timezone_enabled_value_.get();

  if (path == kFineGrainedTimeZoneResolveEnabled)
    return fine_grained_time_zone_enabled_value_.get();

  return NULL;
}

// The timezone is always trusted.
CrosSettingsProvider::TrustedStatus
SystemSettingsProvider::PrepareTrustedValues(base::OnceClosure* cb) {
  return TRUSTED;
}

bool SystemSettingsProvider::HandlesSetting(std::string_view path) const {
  return path == kSystemTimezone || path == kPerUserTimezoneEnabled ||
         path == kFineGrainedTimeZoneResolveEnabled;
}

void SystemSettingsProvider::TimezoneChanged(const icu::TimeZone& timezone) {
  // Fires system setting change notification.
  timezone_value_ = std::make_unique<base::Value>(
      system::TimezoneSettings::GetTimezoneID(timezone));
  NotifyObservers(kSystemTimezone);
}

}  // namespace ash
