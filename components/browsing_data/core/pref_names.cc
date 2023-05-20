// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/pref_names.h"

#include "base/values.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace browsing_data::prefs {

// JSON config to periodically delete some browsing data as specified by
// the BrowsingDataLifetime policy.
const char kBrowsingDataLifetime[] =
    "browser.clear_data.browsing_data_lifetime";

// Boolean set to true while browsing data needs to be deleted per
// ClearBrowsingDataOnExit policy.
// TODO (crbug/1026442): Consider setting this pref to true during fast
// shutdown if the ClearBrowsingDataOnExit policy is set.
const char kClearBrowsingDataOnExitDeletionPending[] =
    "browser.clear_data.clear_on_exit_pending";

// List of browsing data, specified by the ClearBrowsingDataOnExit policy, to
// delete just before browser shutdown.
const char kClearBrowsingDataOnExitList[] = "browser.clear_data.clear_on_exit";

// Clear browsing data deletion time period.
const char kDeleteTimePeriod[] = "browser.clear_data.time_period";
const char kDeleteTimePeriodBasic[] = "browser.clear_data.time_period_basic";

// Clear Browsing Data dialog datatype preferences.
const char kDeleteBrowsingHistory[] = "browser.clear_data.browsing_history";
const char kDeleteBrowsingHistoryBasic[] =
    "browser.clear_data.browsing_history_basic";
const char kDeleteDownloadHistory[] = "browser.clear_data.download_history";
const char kDeleteCache[] = "browser.clear_data.cache";
const char kDeleteCacheBasic[] = "browser.clear_data.cache_basic";
const char kDeleteCookies[] = "browser.clear_data.cookies";
const char kDeleteCookiesBasic[] = "browser.clear_data.cookies_basic";
const char kDeletePasswords[] = "browser.clear_data.passwords";
const char kDeleteFormData[] = "browser.clear_data.form_data";
const char kDeleteHostedAppsData[] = "browser.clear_data.hosted_apps_data";
const char kDeleteSiteSettings[] = "browser.clear_data.site_settings";

// Other Clear Browsing Data preferences.
const char kLastClearBrowsingDataTime[] =
  "browser.last_clear_browsing_data_time";
const char kClearBrowsingDataHistoryNoticeShownTimes[] =
  "browser.clear_data.history_notice_shown_times";
const char kLastClearBrowsingDataTab[] = "browser.last_clear_browsing_data_tab";
const char kPreferencesMigratedToBasic[] =
    "browser.clear_data.preferences_migrated_to_basic";

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

  registry->RegisterIntegerPref(kLastClearBrowsingDataTab, 0);
  registry->RegisterBooleanPref(
      kPreferencesMigratedToBasic, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace browsing_data::prefs
