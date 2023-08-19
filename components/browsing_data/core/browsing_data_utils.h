// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/browsing_data/core/clear_browsing_data_tab.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "net/cookies/cookie_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace browsing_data {

// Browsing data types as seen in the Android and Desktop UI.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
enum class BrowsingDataType {
  HISTORY,
  CACHE,
  COOKIES,
  PASSWORDS,
  FORM_DATA,
  SITE_SETTINGS,
  // Only for Android:
  BOOKMARKS,
  // Only for Desktop:
  DOWNLOADS,
  HOSTED_APPS_DATA,
  NUM_TYPES
};

// Time period ranges available when doing browsing data removals.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
enum class TimePeriod {
  LAST_HOUR = 0,
  LAST_DAY,
  LAST_WEEK,
  FOUR_WEEKS,
  ALL_TIME,
  OLDER_THAN_30_DAYS,
  LAST_15_MINUTES,
  TIME_PERIOD_LAST = LAST_15_MINUTES
};

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
  kMaxValue = kPageInfoResetPermissions,
};

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
absl::optional<BrowsingDataType> GetDataTypeFromDeletionPreference(
    const std::string& pref_name);

bool IsHttpsCookieSourceScheme(net::CookieSourceScheme cookie_source_scheme);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_
