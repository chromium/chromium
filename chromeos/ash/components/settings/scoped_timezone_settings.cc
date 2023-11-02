// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/scoped_timezone_settings.h"

#include <string>

#include "base/check.h"
#include "chromeos/ash/components/settings/timezone_settings.h"

namespace ash::system {
namespace {

TimezoneSettings* GetTimezoneSettingsInstance() {
  auto* timezone_settings = TimezoneSettings::GetInstance();
  DCHECK(timezone_settings);
  return timezone_settings;
}

}  // namespace

ScopedTimezoneSettings::ScopedTimezoneSettings() {
  SaveOriginalTimezone();
}

ScopedTimezoneSettings::ScopedTimezoneSettings(
    const std::u16string& new_timezone_id) {
  SaveOriginalTimezone();
  SetTimezoneFromID(new_timezone_id);
}

ScopedTimezoneSettings::~ScopedTimezoneSettings() {
  GetTimezoneSettingsInstance()->SetTimezoneFromID(original_timezone_id_);
}

std::u16string ScopedTimezoneSettings::GetCurrentTimezoneID() const {
  return GetTimezoneSettingsInstance()->GetCurrentTimezoneID();
}

void ScopedTimezoneSettings::SetTimezoneFromID(
    const std::u16string& new_timezone_id) const {
  GetTimezoneSettingsInstance()->SetTimezoneFromID(new_timezone_id);
}

void ScopedTimezoneSettings::SaveOriginalTimezone() {
  original_timezone_id_ = GetTimezoneSettingsInstance()->GetCurrentTimezoneID();
}

}  // namespace ash::system
