// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_metrics.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {
namespace metrics {

namespace {

// Sync data types that can be customized in settings.
// Used in histograms. Do not change existing values, append new values at the
// end.
enum class SyncDataType {
  kNone = 0,
  kApps = 1,
  kBookmarks = 2,
  kExtensions = 3,
  kHistory = 4,
  kSettings = 5,
  kThemes = 6,
  kTabs = 7,
  kPasswords = 8,
  kAutofill = 9,
  kPayments = 10,
  // kSync = 11,

  kMaxValue = kPayments
};

void RecordSyncDataTypeSample(SyncDataType data_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "UnifiedConsent.SyncAndGoogleServicesSettings.AfterAdvancedOptIn."
      "SyncDataTypesOff",
      data_type);
}

// Checks states of sync data types and records corresponding histogram.
// Returns true if a sample was recorded.
bool RecordSyncSetupDataTypesImpl(syncer::SyncUserSettings* sync_settings) {
  bool metric_recorded = false;

  std::vector<std::pair<SyncDataType, syncer::UserSelectableType>> sync_types;
  sync_types.emplace_back(SyncDataType::kBookmarks,
                          syncer::UserSelectableType::kBookmarks);
  sync_types.emplace_back(SyncDataType::kHistory,
                          syncer::UserSelectableType::kHistory);
  sync_types.emplace_back(SyncDataType::kSettings,
                          syncer::UserSelectableType::kPreferences);
  sync_types.emplace_back(SyncDataType::kTabs,
                          syncer::UserSelectableType::kTabs);
  sync_types.emplace_back(SyncDataType::kPasswords,
                          syncer::UserSelectableType::kPasswords);
  sync_types.emplace_back(SyncDataType::kAutofill,
                          syncer::UserSelectableType::kAutofill);
  sync_types.emplace_back(SyncDataType::kPayments,
                          syncer::UserSelectableType::kPayments);
#if !BUILDFLAG(IS_ANDROID)
  sync_types.emplace_back(SyncDataType::kApps,
                          syncer::UserSelectableType::kApps);
  sync_types.emplace_back(SyncDataType::kExtensions,
                          syncer::UserSelectableType::kExtensions);
  sync_types.emplace_back(SyncDataType::kThemes,
                          syncer::UserSelectableType::kThemes);
#endif

  for (const auto& [bucket, type] : sync_types) {
    if (!sync_settings->GetSelectedTypes().Has(type)) {
      RecordSyncDataTypeSample(bucket);
      metric_recorded = true;
    }
  }

  return metric_recorded;
}

}  // namespace

void RecordSettingsHistogram(PrefService* pref_service) {
  bool is_enabled =
      pref_service->GetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  UMA_HISTOGRAM_BOOLEAN(
      "UnifiedConsent.MakeSearchesAndBrowsingBetter.OnProfileLoad", is_enabled);
}

void RecordSyncSetupDataTypesHistrogam(
    syncer::SyncUserSettings* sync_settings) {
  if (!RecordSyncSetupDataTypesImpl(sync_settings)) {
    RecordSyncDataTypeSample(SyncDataType::kNone);
  }
}

}  // namespace metrics
}  // namespace unified_consent
