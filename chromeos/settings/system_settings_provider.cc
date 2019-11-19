// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/settings/system_settings_provider.h"

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/settings/cros_settings_names.h"

namespace chromeos {
namespace {
// TODO(olsen): PerUserTimeZoneEnabled and FineGrainedTimeZoneDetectionEnabled
// are duplicated in chrome/browser/chromeos/system/timezone_util.cc, which
// is not visible from this package. Try to re-unify these functions by moving
// timezone_util to src/chromeos too (out of src/chrome/browser).

bool PerUserTimezoneEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisablePerUserTimezone);
}

bool FineGrainedTimeZoneDetectionEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableFineGrainedTimeZoneDetection);
}

}  // namespace

SystemSettingsProvider::SystemSettingsProvider()
    : CrosSettingsProvider(CrosSettingsProvider::NotifyObserversCallback()) {
  Init();
}

SystemSettingsProvider::SystemSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  Init();
}

SystemSettingsProvider::~SystemSettingsProvider() {
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void SystemSettingsProvider::Init() {
  system::TimezoneSettings* timezone_settings =
      system::TimezoneSettings::GetInstance();
  timezone_settings->AddObserver(this);
  timezone_value_.reset(
      new base::Value(timezone_settings->GetCurrentTimezoneID()));
  per_user_timezone_enabled_value_.reset(
      new base::Value(PerUserTimezoneEnabled()));
  fine_grained_time_zone_enabled_value_.reset(
      new base::Value(FineGrainedTimeZoneDetectionEnabled()));
}

const base::Value* SystemSettingsProvider::Get(const std::string& path) const {
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
SystemSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  return TRUSTED;
}

bool SystemSettingsProvider::HandlesSetting(const std::string& path) const {
  return path == kSystemTimezone || path == kPerUserTimezoneEnabled ||
         path == kFineGrainedTimeZoneResolveEnabled;
}

void SystemSettingsProvider::TimezoneChanged(const icu::TimeZone& timezone) {
  // Fires system setting change notification.
  timezone_value_.reset(
      new base::Value(system::TimezoneSettings::GetTimezoneID(timezone)));
  NotifyObservers(kSystemTimezone);
}

}  // namespace chromeos
