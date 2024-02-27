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
  registry->RegisterIntegerPref(
      kDeleteTimePeriod, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kDeleteTimePeriodBasic, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kDeleteTimePeriodV2, -1, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kDeleteTimePeriodV2Basic, -1,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteBrowsingHistory, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteBrowsingHistoryBasic, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteCache, true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteCacheBasic, true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteCookies, true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteCookiesBasic, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeletePasswords, false, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteFormData, false, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kClearBrowsingDataHistoryNoticeShownTimes, 0);

#if !BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(
      kDeleteDownloadHistory, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteHostedAppsData, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      kDeleteSiteSettings, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#else
  registry->RegisterInt64Pref(prefs::kLastClearBrowsingDataTime, 0);
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      kCloseTabs, false, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // BUILDFLAG(IS_ANDROID)

  registry->RegisterIntegerPref(kLastClearBrowsingDataTab, 0);
  registry->RegisterBooleanPref(
      kPreferencesMigratedToBasic, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace browsing_data::prefs
