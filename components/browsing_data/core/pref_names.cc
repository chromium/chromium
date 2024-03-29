// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/pref_names.h"

#include "base/values.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace browsing_data::prefs {

void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kBrowsingDataLifetime);
  registry->RegisterBooleanPref(kClearBrowsingDataOnExitDeletionPending, false);
  registry->RegisterListPref(kClearBrowsingDataOnExitList);
  registry->RegisterIntegerPref(kDeleteTimePeriod, 0);
  registry->RegisterIntegerPref(kDeleteTimePeriodBasic, 0);
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

  registry->RegisterIntegerPref(kLastClearBrowsingDataTab, 0);
}

}  // namespace browsing_data::prefs
