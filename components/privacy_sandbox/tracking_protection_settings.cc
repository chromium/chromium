// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/check.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

namespace privacy_sandbox {

TrackingProtectionSettings::TrackingProtectionSettings(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

TrackingProtectionSettings::~TrackingProtectionSettings() = default;

bool TrackingProtectionSettings::IsTrackingProtection3pcdEnabled() const {
  // True if either debug flag or pref is enabled.
  return base::FeatureList::IsEnabled(
             content_settings::features::kTrackingProtection3pcd) ||
         pref_service_->GetBoolean(prefs::kTrackingProtection3pcdEnabled);
}

tracking_protection::TrackingProtectionLevel
TrackingProtectionSettings::GetTrackingProtectionLevel() const {
  return static_cast<tracking_protection::TrackingProtectionLevel>(
      pref_service_->GetInteger(prefs::kTrackingProtectionLevel));
}

bool TrackingProtectionSettings::IsCustomTrackingProtectionLevel() const {
  return GetTrackingProtectionLevel() ==
         tracking_protection::TrackingProtectionLevel::kCustom;
}

bool TrackingProtectionSettings::IsStandardTrackingProtectionLevel() const {
  return GetTrackingProtectionLevel() ==
         tracking_protection::TrackingProtectionLevel::kStandard;
}

bool TrackingProtectionSettings::IsDoNotTrackEnabled() const {
  // If we're not in custom mode, treat DNT as false.
  if (IsTrackingProtection3pcdEnabled() && !IsCustomTrackingProtectionLevel()) {
    return false;
  }
  return pref_service_->GetBoolean(prefs::kEnableDoNotTrack);
}

}  // namespace privacy_sandbox
