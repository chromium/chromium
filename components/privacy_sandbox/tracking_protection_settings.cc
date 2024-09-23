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
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"
#include "url/gurl.h"

namespace privacy_sandbox {

TrackingProtectionSettings::TrackingProtectionSettings(
    PrefService* pref_service,
    HostContentSettingsMap* host_content_settings_map,
    bool is_incognito)
    : pref_service_(pref_service),
      host_content_settings_map_(host_content_settings_map),
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

  MaybeInitializeIppPref();
  // It's possible enterprise status changed while profile was shut down.
  OnEnterpriseControlForPrefsChanged();

  // If feature status changed then we need to migrate content settings.
  if (base::FeatureList::IsEnabled(kTrackingProtectionContentSettingFor3pcb) &&
      !pref_service_->GetBoolean(prefs::kUserBypass3pcExceptionsMigrated)) {
    MigrateUserBypassExceptions(ContentSettingsType::COOKIES,
                                ContentSettingsType::TRACKING_PROTECTION);
    pref_service_->SetBoolean(prefs::kUserBypass3pcExceptionsMigrated, true);
  } else if (!base::FeatureList::IsEnabled(
                 kTrackingProtectionContentSettingFor3pcb) &&
             pref_service_->GetBoolean(
                 prefs::kUserBypass3pcExceptionsMigrated)) {
    MigrateUserBypassExceptions(ContentSettingsType::TRACKING_PROTECTION,
                                ContentSettingsType::COOKIES);
    pref_service_->SetBoolean(prefs::kUserBypass3pcExceptionsMigrated, false);
  }
}

TrackingProtectionSettings::~TrackingProtectionSettings() = default;

void TrackingProtectionSettings::Shutdown() {
  observers_.Clear();
  host_content_settings_map_ = nullptr;
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
         base::FeatureList::IsEnabled(kIpProtectionV1);
}

bool TrackingProtectionSettings::IsDoNotTrackEnabled() const {
  return pref_service_->GetBoolean(prefs::kEnableDoNotTrack);
}

void TrackingProtectionSettings::AddTrackingProtectionException(
    const GURL& first_party_url,
    bool is_user_bypass_exception) {
  content_settings::ContentSettingConstraints constraints;
  if (is_user_bypass_exception) {
    constraints.set_lifetime(
        content_settings::features::kUserBypassUIExceptionExpiration.Get());
  }

  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url),
      ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_ALLOW,
      constraints);
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

ContentSetting TrackingProtectionSettings::GetTrackingProtectionSetting(
    const GURL& first_party_url,
    content_settings::SettingInfo* info) const {
  return host_content_settings_map_->GetContentSetting(
      GURL(), first_party_url, ContentSettingsType::TRACKING_PROTECTION, info);
}

void TrackingProtectionSettings::MaybeInitializeIppPref() {
  if (pref_service_->GetBoolean(prefs::kIpProtectionInitializedByDogfood) ||
      !base::FeatureList::IsEnabled(kIpProtectionDogfoodDefaultOn)) {
    return;
  }
  pref_service_->SetBoolean(prefs::kIpProtectionEnabled, true);
  pref_service_->SetBoolean(prefs::kIpProtectionInitializedByDogfood, true);
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

void TrackingProtectionSettings::MigrateUserBypassExceptions(
    ContentSettingsType from,
    ContentSettingsType to) {
  // Gives us a bit of padding and there's no need to migrate an exception
  // expiring within the next 5 minutes.
  const base::Time now = base::Time::Now() + base::Minutes(5);
  ContentSettingsForOneType existing_exceptions =
      host_content_settings_map_->GetSettingsForOneType(from);
  for (auto exception : existing_exceptions) {
    // Ensure the exception comes from user bypass.
    if (exception.metadata.expiration() <= now ||
        !exception.primary_pattern.MatchesAllHosts() ||
        exception.secondary_pattern.MatchesAllHosts() ||
        exception.setting_value != CONTENT_SETTING_ALLOW) {
      continue;
    }
    // Add an exception for the type we're migrating to.
    content_settings::ContentSettingConstraints constraints;
    constraints.set_lifetime(exception.metadata.expiration() -
                             base::Time::Now());
    host_content_settings_map_->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(), exception.secondary_pattern, to,
        CONTENT_SETTING_ALLOW, constraints);
    // Remove the exception for the type we're migrating from.
    host_content_settings_map_->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(), exception.secondary_pattern, from,
        CONTENT_SETTING_DEFAULT);
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
