// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
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
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "net/base/features.h"
#include "url/gurl.h"

namespace privacy_sandbox {

TrackingProtectionSettings::TrackingProtectionSettings(
    PrefService* pref_service,
    bool is_incognito)
    : pref_service_(pref_service),
      is_incognito_(is_incognito) {
  CHECK(pref_service_);

  pref_change_registrar_.Init(pref_service_);
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

void TrackingProtectionSettings::Shutdown() {
  observers_.Clear();
  pref_change_registrar_.Reset();
  pref_service_ = nullptr;
}

bool TrackingProtectionSettings::IsTrackingProtection3pcdEnabled() const {
  return base::FeatureList::IsEnabled(
      content_settings::features::kTrackingProtection3pcd);
}

bool TrackingProtectionSettings::AreAllThirdPartyCookiesBlocked() const {
  return IsTrackingProtection3pcdEnabled() &&
         (pref_service_->GetBoolean(prefs::kBlockAll3pcToggleEnabled) ||
          is_incognito_);
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
