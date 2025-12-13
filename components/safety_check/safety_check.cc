// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/safety_check.h"

#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {

const base::TimeDelta kUnusedSitePermissionsRevocationCleanUpThreshold =
    base::Days(30);

}  // namespace

namespace safety_check {

base::TimeDelta GetUnusedSitePermissionsRevocationCleanUpThreshold() {
  return kUnusedSitePermissionsRevocationCleanUpThreshold;
}

SafeBrowsingStatus CheckSafeBrowsing(PrefService* pref_service) {
  const PrefService::Preference* enabled_pref =
      pref_service->FindPreference(prefs::kSafeBrowsingEnabled);
  bool is_sb_enabled = pref_service->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool is_sb_managed = enabled_pref->IsManaged();

  if (is_sb_enabled && pref_service->GetBoolean(prefs::kSafeBrowsingEnhanced))
    return SafeBrowsingStatus::kEnabledEnhanced;
  if (is_sb_enabled && is_sb_managed)
    return SafeBrowsingStatus::kEnabledStandard;
  if (is_sb_enabled && !is_sb_managed)
    return SafeBrowsingStatus::kEnabledStandardAvailableEnhanced;
  if (is_sb_managed)
    return SafeBrowsingStatus::kDisabledByAdmin;
  if (enabled_pref->IsExtensionControlled())
    return SafeBrowsingStatus::kDisabledByExtension;
  return SafeBrowsingStatus::kDisabled;
}

}  // namespace safety_check
