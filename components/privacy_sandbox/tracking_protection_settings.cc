// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/check.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

namespace privacy_sandbox {

TrackingProtectionSettings::TrackingProtectionSettings(
    PrefService* pref_service,
    TrackingProtectionOnboarding* onboarding_service)
    : pref_service_(pref_service), onboarding_service_(onboarding_service) {
  CHECK(pref_service_);
  if (onboarding_service_) {
    onboarding_observation_.Observe(onboarding_service_);
  }

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kEnableDoNotTrack,
      base::BindRepeating(
          &TrackingProtectionSettings::OnDoNotTrackEnabledPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kTrackingProtectionLevel,
      base::BindRepeating(
          &TrackingProtectionSettings::OnTrackingProtectionLevelPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kBlockAll3pcToggleEnabled,
      base::BindRepeating(
          &TrackingProtectionSettings::OnBlockAllThirdPartyCookiesPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kTrackingProtection3pcdEnabled,
      base::BindRepeating(
          &TrackingProtectionSettings::OnTrackingProtection3pcdPrefChanged,
          base::Unretained(this)));
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

bool TrackingProtectionSettings::AreAllThirdPartyCookiesBlocked() const {
  return IsCustomTrackingProtectionLevel() &&
         pref_service_->GetBoolean(prefs::kBlockAll3pcToggleEnabled);
}

bool TrackingProtectionSettings::IsDoNotTrackEnabled() const {
  // If we're not in custom mode, treat DNT as false.
  if (IsTrackingProtection3pcdEnabled() && !IsCustomTrackingProtectionLevel()) {
    return false;
  }
  return pref_service_->GetBoolean(prefs::kEnableDoNotTrack);
}

void TrackingProtectionSettings::OnTrackingProtectionOnboarded() {
  pref_service_->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
}

void TrackingProtectionSettings::OnDoNotTrackEnabledPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnDoNotTrackEnabledChanged();
  }
}

void TrackingProtectionSettings::OnTrackingProtectionLevelPrefChanged() {
  for (auto& observer : observers_) {
    if (pref_service_->GetBoolean(prefs::kEnableDoNotTrack)) {
      observer.OnDoNotTrackEnabledChanged();
    }
    if (pref_service_->GetBoolean(prefs::kBlockAll3pcToggleEnabled)) {
      observer.OnBlockAllThirdPartyCookiesChanged();
    }
  }
}

void TrackingProtectionSettings::OnBlockAllThirdPartyCookiesPrefChanged() {
  for (auto& observer : observers_) {
    // Note: this pref can only be changed in the custom level
    observer.OnBlockAllThirdPartyCookiesChanged();
  }
}

void TrackingProtectionSettings::OnTrackingProtection3pcdPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnDoNotTrackEnabledChanged();
    observer.OnBlockAllThirdPartyCookiesChanged();
  }
}

void TrackingProtectionSettings::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrackingProtectionSettings::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace privacy_sandbox
