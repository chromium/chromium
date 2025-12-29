// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace privacy_sandbox {

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
  const int kBlockThirdParty =
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty);
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
