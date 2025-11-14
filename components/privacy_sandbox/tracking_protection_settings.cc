// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
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

// This logic accesses prefs that aren't registered on iOS.
#if !BUILDFLAG(IS_IOS)
  // It's possible enterprise status changed while profile was shut down.
  OnEnterpriseControlForPrefsChanged();
  // Set Mode B pref to force rollback flow.
  if (privacy_sandbox::kRollBackModeBForced.Get()) {
    pref_service_->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  }
#endif
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

void MaybeSetRollbackPrefsModeB(syncer::SyncService* sync_service,
                                PrefService* prefs) {
  // Only set prefs if:
  // 1. User is in Mode B and rollback feature is enabled.
  if (!prefs->GetBoolean(prefs::kTrackingProtection3pcdEnabled) ||
      !base::FeatureList::IsEnabled(kRollBackModeB)) {
    return;
  }
  // 2. We are not waiting for pref sync updates.
  if (sync_service && sync_service->IsSyncFeatureEnabled() &&
      sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPreferences) &&
      sync_service->GetDownloadStatusFor(syncer::DataType::PREFERENCES) ==
          syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates) {
    return;
  }

  // Hardcoded as using CookieControlsMode creates a circular dependency.
  const int kBlockThirdParty = 1;
  bool allowed_3pcs =
      !prefs->GetBoolean(prefs::kBlockAll3pcToggleEnabled) &&
      prefs->GetInteger(prefs::kCookieControlsMode) != kBlockThirdParty;
  if (!allowed_3pcs) {
    prefs->SetInteger(prefs::kCookieControlsMode, kBlockThirdParty);
  }
  // If 3PCs are allowed then we should show the notice.
  prefs->SetBoolean(prefs::kShowRollbackUiModeB, allowed_3pcs);
  base::UmaHistogramBoolean("Privacy.3PCD.RollbackNotice.ShouldShow",
                            allowed_3pcs);
  prefs->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
}

}  // namespace privacy_sandbox
