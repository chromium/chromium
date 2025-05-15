// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"
#include "net/base/features.h"
#include "url/gurl.h"

namespace privacy_sandbox {

TrackingProtectionSettings::TrackingProtectionSettings(
    PrefService* pref_service,
    HostContentSettingsMap* host_content_settings_map,
    policy::ManagementService* management_service,
    bool is_incognito)
    : pref_service_(pref_service),
      host_content_settings_map_(host_content_settings_map),
      management_service_(management_service),
      is_incognito_(is_incognito) {
  CHECK(pref_service_);
  CHECK(host_content_settings_map_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kEnableDoNotTrack,
      base::BindRepeating(
          &TrackingProtectionSettings::OnDoNotTrackEnabledPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kIpProtectionEnabled,
      base::BindRepeating(
          &TrackingProtectionSettings::OnIpProtectionPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kFingerprintingProtectionEnabled,
      base::BindRepeating(
          &TrackingProtectionSettings::OnFpProtectionPrefChanged,
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
  // For enterprise status
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(
          &TrackingProtectionSettings::OnEnterpriseControlForPrefsChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
      base::BindRepeating(
          &TrackingProtectionSettings::OnEnterpriseControlForPrefsChanged,
          base::Unretained(this)));

  // It's possible enterprise status changed while profile was shut down.
  OnEnterpriseControlForPrefsChanged();
}

TrackingProtectionSettings::~TrackingProtectionSettings() = default;

void TrackingProtectionSettings::Shutdown() {
  observers_.Clear();
  host_content_settings_map_ = nullptr;
  management_service_ = nullptr;
  pref_change_registrar_.Reset();
  pref_service_ = nullptr;
}

bool TrackingProtectionSettings::IsTrackingProtection3pcdEnabled() const {
  // True if either debug flag or pref is enabled.
  return base::FeatureList::IsEnabled(
             content_settings::features::kTrackingProtection3pcd) ||
         pref_service_->GetBoolean(prefs::kTrackingProtection3pcdEnabled);
}

bool TrackingProtectionSettings::AreAllThirdPartyCookiesBlocked() const {
  return IsTrackingProtection3pcdEnabled() &&
         (pref_service_->GetBoolean(prefs::kBlockAll3pcToggleEnabled) ||
          is_incognito_);
}

bool TrackingProtectionSettings::IsIpProtectionEnabled() const {
  return pref_service_->GetBoolean(prefs::kIpProtectionEnabled) &&
         base::FeatureList::IsEnabled(kIpProtectionUx);
}

bool TrackingProtectionSettings::IsFpProtectionEnabled() const {
  return pref_service_->GetBoolean(prefs::kFingerprintingProtectionEnabled) &&
         is_incognito_ &&
         base::FeatureList::IsEnabled(kFingerprintingProtectionUx);
}

bool TrackingProtectionSettings::IsDoNotTrackEnabled() const {
  return pref_service_->GetBoolean(prefs::kEnableDoNotTrack);
}

void TrackingProtectionSettings::AddTrackingProtectionException(
    const GURL& first_party_url) {
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url),
      ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_ALLOW);
}

void TrackingProtectionSettings::RemoveTrackingProtectionException(
    const GURL& first_party_url) {
  // Exceptions added via `AddTrackingProtectionException` are site scoped. This
  // resets both origin scoped and site scoped exceptions.
  auto pattern =
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url);
  content_settings::SettingInfo info;
  host_content_settings_map_->GetContentSetting(
      GURL(), first_party_url, ContentSettingsType::TRACKING_PROTECTION, &info);
  if (!info.secondary_pattern.HasDomainWildcard()) {
    pattern = info.secondary_pattern;
  }
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), pattern,
      ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_DEFAULT);
}

bool TrackingProtectionSettings::HasTrackingProtectionException(
    const GURL& first_party_url,
    content_settings::SettingInfo* info) const {
  return host_content_settings_map_->GetContentSetting(
             GURL(), first_party_url, ContentSettingsType::TRACKING_PROTECTION,
             info) == CONTENT_SETTING_ALLOW;
}

bool TrackingProtectionSettings::IsIpProtectionDisabledForEnterprise() {
  if (pref_service_->IsManagedPreference(prefs::kIpProtectionEnabled)) {
    return !pref_service_->GetBoolean(prefs::kIpProtectionEnabled);
  }
  if (net::features::kIpPrivacyDisableForEnterpriseByDefault.Get()) {
    // Disable IP Protection for managed profiles and managed devices when the
    // admins haven't explicitly opted in to it via enterprise policy.
    return management_service_->IsManaged() ||
           policy::PlatformManagementService::GetInstance()->IsManaged();
  }
  return false;
}

// TODO(https://b/333527273): Delete with Mode B cleanup
void TrackingProtectionSettings::OnEnterpriseControlForPrefsChanged() {
  if (!IsTrackingProtection3pcdEnabled()) {
    return;
  }
  // Stop showing users new UX and using new prefs if old prefs become managed.
  if (pref_service_->IsManagedPreference(prefs::kCookieControlsMode) ||
      pref_service_->IsManagedPreference(
          prefs::kPrivacySandboxRelatedWebsiteSetsEnabled)) {
    pref_service_->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  }
}

void TrackingProtectionSettings::OnDoNotTrackEnabledPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnDoNotTrackEnabledChanged();
  }
}

void TrackingProtectionSettings::OnIpProtectionPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnIpProtectionEnabledChanged();
  }
}

void TrackingProtectionSettings::OnFpProtectionPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnFpProtectionEnabledChanged();
  }
}

void TrackingProtectionSettings::OnBlockAllThirdPartyCookiesPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnBlockAllThirdPartyCookiesChanged();
  }
}

void TrackingProtectionSettings::OnTrackingProtection3pcdPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnTrackingProtection3pcdChanged();
    // 3PC blocking may change as a result of entering/leaving the experiment.
    observer.OnBlockAllThirdPartyCookiesChanged();
  }
}

void TrackingProtectionSettings::AddObserver(
    TrackingProtectionSettingsObserver* observer) {
  observers_.AddObserver(observer);
}

void TrackingProtectionSettings::RemoveObserver(
    TrackingProtectionSettingsObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace privacy_sandbox
