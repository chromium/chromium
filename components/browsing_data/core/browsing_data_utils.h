// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/browsing_data/core/clear_browsing_data_tab.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "net/cookies/cookie_constants.h"

namespace browsing_data {

// Histogram name for when an action happens in Delete Browsing Data dialog used
// in all platforms.
extern const char kDeleteBrowsingDataDialogHistogram[];

// Browsing data types as seen in the Android and Desktop UI. Keep in sync with
// the respective enum in
// c/b/r/s/clear_browsing_data_dialog/clear_browsing_data_browser_proxy.ts
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
// LINT.IfChange(BrowsingDataType)
enum class BrowsingDataType {
  HISTORY = 0,
  CACHE = 1,
  SITE_DATA = 2,
  PASSWORDS = 3,
  FORM_DATA = 4,
  SITE_SETTINGS = 5,
  // Only for Desktop:
  DOWNLOADS = 6,
  HOSTED_APPS_DATA = 7,
  // Only for Android:
  TABS = 8,
  MAX_VALUE = TABS,
};
// LINT.ThenChange(/chrome/browser/resources/settings/clear_browsing_data_dialog/clear_browsing_data_browser_proxy.ts:BrowsingDataType)

// Time period ranges available when doing browsing data removals.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with respective enums in
// tools/metrics/histograms/metadata/settings/enums.xml and
// c/b/r/s/clear_browsing_data_dialog/clear_browsing_data_browser_proxy.ts
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
// LINT.IfChange(TimePeriod)
enum class TimePeriod {
  LAST_HOUR = 0,
  LAST_DAY = 1,
  LAST_WEEK = 2,
  FOUR_WEEKS = 3,
  ALL_TIME = 4,
  OLDER_THAN_30_DAYS = 5,
  LAST_15_MINUTES = 6,
  TIME_PERIOD_LAST = LAST_15_MINUTES
};
// LINT.ThenChange(/chrome/browser/resources/settings/clear_browsing_data_dialog/clear_browsing_data_browser_proxy.ts:TimePeriod)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must be kept in sync with the DeleteBrowsingDataAction in enums.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.browsing_data
//
// Note: Make sure to keep in sync with DeleteBrowsingDataAction defined in
//   chrome/browser/resources/settings/site_settings/metrics_browser_proxy.ts
enum class DeleteBrowsingDataAction {
  kClearBrowsingDataDialog = 0,
  kClearBrowsingDataOnExit = 1,
  kIncognitoCloseTabs = 2,
  kCookiesInUseDialog = 3,
  kSitesSettingsPage = 4,
  kHistoryPageEntries = 5,
  kQuickDelete = 6,
  kPageInfoResetPermissions = 7,
  kRwsDeleteAllData = 8,
  kMaxValue = kRwsDeleteAllData,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DeleteBrowsingDataDialogAction)
enum class DeleteBrowsingDataDialogAction {
  kBrowsingHistoryToggledOn = 0,
  kBrowsingHistoryToggledOff = 1,
  kTabsToggledOn = 2,
  kTabsToggledOff = 3,
  kSiteDataToggledOn = 4,
  kSiteDataToggledOff = 5,
  kCacheToggledOn = 6,
  kCacheToggledOff = 7,
  kPasswordsToggledOn = 8,
  kPasswordsToggledOff = 9,
  kAutofillToggledOn = 10,
  kAutofillToggledOff = 11,
  kUpdateDataTypesSelected = 12,
  kCancelDataTypesSelected = 13,
  kSignoutLinkOpened = 14,
  kLast15MinutesSelected = 15,
  kLastHourSelected = 16,
  kLastDaySelected = 17,
  kLastWeekSelected = 18,
  kLastFourWeeksSelected = 19,
  kOlderThan30DaysSelected = 20,
  kAllTimeSelected = 21,
  kBrowsingDataSelected = 22,
  kSearchHistoryLinkOpened = 23,
  kMyActivityLinkedOpened = 24,
  kDeletionSelected = 25,
  kCancelSelected = 26,
  kDialogDismissedImplicitly = 27,
  kMenuItemEntryPointSelected = 28,
  kHistoryEntryPointSelected = 29,
  kPrivacyEntryPointSelected = 30,
  kKeyboardEntryPointSelected = 31,
  kMaxValue = kKeyboardEntryPointSelected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:DeleteBrowsingDataDialogAction)

// Calculate the begin time for the deletion range specified by |time_period|.
base::Time CalculateBeginDeleteTime(TimePeriod time_period);

// Calculate the end time for the deletion range specified by |time_period|.
base::Time CalculateEndDeleteTime(TimePeriod time_period);

// Records the UMA action of UI-triggered data deletion for |time_period|.
void RecordDeletionForPeriod(TimePeriod time_period);

// Records the UMA action of a change of the clear browsing data time period.
void RecordTimePeriodChange(TimePeriod period);

// Record Delete Browsing Data Action specified by |cbd_action|.
void RecordDeleteBrowsingDataAction(DeleteBrowsingDataAction cbd_action);

// Constructs the text to be displayed by a counter from the given |result|.
// Currently this can only be used for counters for which the Result is
// defined in components/browsing_data/core/counters.
std::u16string GetCounterTextFromResult(
    const BrowsingDataCounter::Result* result);

// Returns the preference that stores the time period.
const char* GetTimePeriodPreferenceName(
    ClearBrowsingDataTab clear_browsing_data_tab);

// Copies the name of the deletion preference corresponding to the given
// |data_type| to |out_pref|. Returns false if no such preference exists.
bool GetDeletionPreferenceFromDataType(
    BrowsingDataType data_type,
    ClearBrowsingDataTab clear_browsing_data_tab,
    std::string* out_pref);

// Returns a BrowsingDataType if a type matching |pref_name| is found.
std::optional<BrowsingDataType> GetDataTypeFromDeletionPreference(
    const std::string& pref_name);

bool IsHttpsCookieSourceScheme(net::CookieSourceScheme cookie_source_scheme);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_
