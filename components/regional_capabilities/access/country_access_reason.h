// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_ACCESS_COUNTRY_ACCESS_REASON_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_ACCESS_COUNTRY_ACCESS_REASON_H_

namespace regional_capabilities {

// Keys for `CountryIdHolder::GetRestricted()`.
enum class CountryAccessReason {
  // TODO(crbug.com/328040066): To be removed when the migration away from
  // `SearchEngineChoiceService::GetCountryId()` is done.
  kSearchEngineChoiceServiceDeprecatedForwardCall,

  // Used to check whether the current country is in scope for re-triggering
  // the search engine choice screen.
  // Added with the initial access control migration, see crbug.com/328040066.
  kSearchEngineChoiceServiceReprompting,

  // Used to obtain the country associated with the choice screen that has
  // just been shown, when metrics reporting needs to be delayed.
  // Added with the initial access control migration, see crbug.com/328040066.
  kSearchEngineChoiceServiceCacheChoiceScreenData,

  // Used for computing of the list of prepopulated search engines.
  // Added with the initial access control migration, see crbug.com/328040066.
  kTemplateURLPrepopulateDataResolution,
};

// Pass key inspired from `base::NonCopyablePassKey` that also allows specifying
// an access reason, for more granularity than class-level access control.
template <typename T>
class CountryAccessKey {
 public:
  CountryAccessKey(const CountryAccessKey&) = delete;
  CountryAccessKey& operator=(const CountryAccessKey&) = delete;

  const CountryAccessReason reason;

 private:
  friend T;
  explicit CountryAccessKey(CountryAccessReason reason) : reason(reason) {}
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_ACCESS_COUNTRY_ACCESS_REASON_H_
