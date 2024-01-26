// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_PREF_NAMES_H_
#define COMPONENTS_BROWSING_DATA_CORE_PREF_NAMES_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace browsing_data::prefs {

// JSON config to periodically delete some browsing data as specified by
// the BrowsingDataLifetime policy.
inline constexpr char kBrowsingDataLifetime[] =
    "browser.clear_data.browsing_data_lifetime";

// Boolean set to true while browsing data needs to be deleted per
// ClearBrowsingDataOnExit policy.
// TODO (crbug/1026442): Consider setting this pref to true during fast
// shutdown if the ClearBrowsingDataOnExit policy is set.
inline constexpr char kClearBrowsingDataOnExitDeletionPending[] =
    "browser.clear_data.clear_on_exit_pending";

// List of browsing data, specified by the ClearBrowsingDataOnExit policy, to
// delete just before browser shutdown.
inline constexpr char kClearBrowsingDataOnExitList[] =
    "browser.clear_data.clear_on_exit";

// Clear browsing data deletion time period.
inline constexpr char kDeleteTimePeriod[] = "browser.clear_data.time_period";
inline constexpr char kDeleteTimePeriodBasic[] =
    "browser.clear_data.time_period_basic";

// Clear browsing data deletion time period experiment. This experiment requires
// users to interact with timeframe drop down menu in the clear browsing data
// dialog. It also adds a new 'Last 15 minutes' value to the list. Until the
// user has made their 1st time period selection, the UI shows 'Select a time
// range'.
inline constexpr char kDeleteTimePeriodV2[] =
    "browser.clear_data.time_period_v2";
inline constexpr char kDeleteTimePeriodV2Basic[] =
    "browser.clear_data.time_period_v2_basic";

// Clear Browsing Data dialog datatype preferences.
inline constexpr char kDeleteBrowsingHistory[] =
    "browser.clear_data.browsing_history";
inline constexpr char kDeleteBrowsingHistoryBasic[] =
    "browser.clear_data.browsing_history_basic";
inline constexpr char kDeleteDownloadHistory[] =
    "browser.clear_data.download_history";
inline constexpr char kDeleteCache[] = "browser.clear_data.cache";
inline constexpr char kDeleteCacheBasic[] = "browser.clear_data.cache_basic";
inline constexpr char kDeleteCookies[] = "browser.clear_data.cookies";
inline constexpr char kDeleteCookiesBasic[] =
    "browser.clear_data.cookies_basic";
inline constexpr char kDeletePasswords[] = "browser.clear_data.passwords";
inline constexpr char kDeleteFormData[] = "browser.clear_data.form_data";
inline constexpr char kDeleteHostedAppsData[] =
    "browser.clear_data.hosted_apps_data";
inline constexpr char kDeleteSiteSettings[] =
    "browser.clear_data.site_settings";

// Other Clear Browsing Data preferences.
inline constexpr char kLastClearBrowsingDataTime[] =
    "browser.last_clear_browsing_data_time";
inline constexpr char kClearBrowsingDataHistoryNoticeShownTimes[] =
    "browser.clear_data.history_notice_shown_times";
inline constexpr char kLastClearBrowsingDataTab[] =
    "browser.last_clear_browsing_data_tab";
inline constexpr char kPreferencesMigratedToBasic[] =
    "browser.clear_data.preferences_migrated_to_basic";

// Registers the Clear Browsing Data UI prefs.
void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace browsing_data::prefs

#endif  // COMPONENTS_BROWSING_DATA_CORE_PREF_NAMES_H_
