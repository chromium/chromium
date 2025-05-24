// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_ACCESS_COUNTRY_ACCESS_REASON_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_ACCESS_COUNTRY_ACCESS_REASON_H_

#include "base/gtest_prod_util.h"

class TemplateURLService;
class ProfileInternalsHandler;
class SearchEngineChoiceDialogService;

namespace search_engines {
class SearchEngineChoiceService;
}

namespace regional_capabilities {

// Keys for `CountryIdHolder::GetRestricted()`.
enum class CountryAccessReason {
  // Used to check whether the current country is in scope for re-triggering
  // the search engine choice screen.
  // Added with the initial access control migration, see crbug.com/328040066.
  kSearchEngineChoiceServiceReprompting,

  // Used to obtain the country associated with the choice screen that has
  // just been shown, when metrics reporting needs to be delayed.
  // Added with the initial access control migration, see crbug.com/328040066.
  kSearchEngineChoiceServiceCacheChoiceScreenData,

  // Used to determine whether the local database of search engines needs to
  // be refreshed with the latest prepopulated data set. The value obtained
  // from this access will be cached in the DB to compared later with the
  // current client state.
  // Added with the initial access control migration, see crbug.com/328040066.
  kTemplateURLServiceDatabaseMetadataCaching,

  // Used to print the profile country in the `chrome://profile-internals`
  // debug page, which intends to help investigate b:380002162.
  // Added with the initial access control migration, see crbug.com/328040066.
  kProfileInternalsDisplayInDebugUi,

  // Used in crash debug keys related to investigating crbug.com/318824817.
  // Added with the initial access control migration, see crbug.com/328040066.
  // TODO(crbug.com/318824817): Remove when the bug root cause is found.
  kSearchEngineChoiceNotifyChoiceMadeDebug,
};

// Pass key inspired from `base::NonCopyablePassKey` that also allows specifying
// an access reason, for more granularity than class-level access control.
class CountryAccessKey {
 public:
  CountryAccessKey(const CountryAccessKey&) = delete;
  CountryAccessKey& operator=(const CountryAccessKey&) = delete;

  const CountryAccessReason reason;

 private:
  friend class search_engines::SearchEngineChoiceService;
  friend class RegionalCapabilitiesService;
  friend class ::TemplateURLService;
  friend class ::ProfileInternalsHandler;
  friend class ::SearchEngineChoiceDialogService;
  FRIEND_TEST_ALL_PREFIXES(RegionalCapabilitiesCountryIdTest, GetRestricted);

  explicit CountryAccessKey(CountryAccessReason reason) : reason(reason) {}
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_ACCESS_COUNTRY_ACCESS_REASON_H_
