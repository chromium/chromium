// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/pref_names.h"

#include "base/values.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace browsing_data::prefs {

void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kBrowsingDataLifetime);
  registry->RegisterBooleanPref(kClearBrowsingDataOnExitDeletionPending, false);
  registry->RegisterListPref(kClearBrowsingDataOnExitList);
#if BUILDFLAG(IS_IOS)
  registry->RegisterIntegerPref(
      kDeleteTimePeriod,
      static_cast<int>(browsing_data::TimePeriod::LAST_15_MINUTES));
#else
  registry->RegisterIntegerPref(
      kDeleteTimePeriod,
      static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
#endif  // BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(kDeleteBrowsingHistory, true);
  registry->RegisterBooleanPref(kDeleteCache, true);
  registry->RegisterBooleanPref(kDeleteCookies, true);
  registry->RegisterBooleanPref(kDeletePasswords, false);
  registry->RegisterBooleanPref(kDeleteFormData, false);
  registry->RegisterIntegerPref(kClearBrowsingDataHistoryNoticeShownTimes, 0);

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
#endif  // BUILDFLAG(IS_IOS)

  registry->RegisterBooleanPref(kQuickDeleteEverUsed, false);
}

}  // namespace browsing_data::prefs
