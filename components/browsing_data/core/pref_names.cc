// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/pref_names.h"

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_IOS)
namespace {

// Returns true if all prefs (except tabs) used for the delete browsing data
// selection still use their default value and have not been modified. These are
// the prefs used for the advanced mode, or, in case of iOS, the only prefs
// used.
bool AreAllSelectionPrefsDefaultValue(PrefService* pref_service) {
  const std::string_view selection_pref_names[] = {
      browsing_data::prefs::kDeleteTimePeriod,
      browsing_data::prefs::kDeleteBrowsingHistory,
      browsing_data::prefs::kDeleteCache,
      browsing_data::prefs::kDeleteCookies,
      browsing_data::prefs::kDeletePasswords,
      browsing_data::prefs::kDeleteFormData};

  for (std::string_view pref_name : selection_pref_names) {
    if (!pref_service->FindPreference(pref_name)->IsDefaultValue()) {
      return false;
    }
  }
  return true;
}

}  // namespace
#endif  // BUILDFLAG(IS_IOS)

namespace browsing_data::prefs {

void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kBrowsingDataLifetime);
  registry->RegisterBooleanPref(kClearBrowsingDataOnExitDeletionPending, false);
  registry->RegisterListPref(kClearBrowsingDataOnExitList);
  // TODO(crbug.com/335387869): When MaybeMigrateToQuickDeletePrefValues is
  // removed, set default value in iOS for the `kDeleteTimePeriod` pref to 15
  // minutes.
  registry->RegisterIntegerPref(
      kDeleteTimePeriod,
      static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
  registry->RegisterIntegerPref(
      kDeleteTimePeriodBasic,
      static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
  registry->RegisterIntegerPref(kDeleteTimePeriodV2, -1);
  registry->RegisterIntegerPref(kDeleteTimePeriodV2Basic, -1);
  registry->RegisterBooleanPref(kDeleteBrowsingHistory, true);
  registry->RegisterBooleanPref(kDeleteBrowsingHistoryBasic, true);
  registry->RegisterBooleanPref(kDeleteCache, true);
  registry->RegisterBooleanPref(kDeleteCacheBasic, true);
  registry->RegisterBooleanPref(kDeleteCookies, true);
  registry->RegisterBooleanPref(kDeleteCookiesBasic, true);
  registry->RegisterBooleanPref(kDeletePasswords, false);
  registry->RegisterBooleanPref(kDeleteFormData, false);
  registry->RegisterIntegerPref(
      kClearBrowsingDataHistoryNoticeShownTimes, 0);

#if !BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(kDeleteDownloadHistory, true);
  registry->RegisterBooleanPref(kDeleteHostedAppsData, false);
  registry->RegisterBooleanPref(kDeleteSiteSettings, false);
#else
  registry->RegisterInt64Pref(prefs::kLastClearBrowsingDataTime, 0);
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(kCloseTabs, false);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(kCloseTabs, true);
  registry->RegisterBooleanPref(kMigratedToQuickDeletePrefValues, false);
#endif  // BUILDFLAG(IS_IOS)

  registry->RegisterIntegerPref(kLastClearBrowsingDataTab, 0);
}

#if BUILDFLAG(IS_IOS)

void MaybeMigrateToQuickDeletePrefValues(PrefService* pref_service) {
  bool migratedToQuickDeletePrefValues =
      pref_service->GetBoolean(kMigratedToQuickDeletePrefValues);

  if (migratedToQuickDeletePrefValues) {
    return;
  }

  bool migrateToNewDefaults = AreAllSelectionPrefsDefaultValue(pref_service);

  if (migrateToNewDefaults) {
    pref_service->SetInteger(
        kDeleteTimePeriod,
        static_cast<int>(browsing_data::TimePeriod::LAST_15_MINUTES));
    pref_service->SetBoolean(kCloseTabs, true);
  } else {
    pref_service->SetBoolean(kCloseTabs, false);
  }

  UMA_HISTOGRAM_BOOLEAN("Privacy.DeleteBrowsingData.MigratedToNewDefaults",
                        migrateToNewDefaults);

  pref_service->SetBoolean(kMigratedToQuickDeletePrefValues, true);
}

#endif  // BUILDFLAG(IS_IOS)

}  // namespace browsing_data::prefs
