// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_COUNTRY_ID_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_COUNTRY_ID_H_

#include "base/gtest_prod_util.h"

namespace search_engines {
class SearchEngineChoiceService;
}
namespace TemplateURLPrepopulateData {
class Resolver;
}

namespace regional_capabilities {

class RegionalCapabilitiesService;

template <typename T>
class CountryAccessKey;

enum class CountryAccessReason;

// See `//components/country_codes` for the Country ID format.
using CountryId = int;

class CountryIdHolder final {
 public:
  explicit CountryIdHolder(CountryId country_id);

  CountryIdHolder(const CountryIdHolder& other);
  CountryIdHolder& operator=(const CountryIdHolder& other);

  ~CountryIdHolder();

  bool operator==(const CountryIdHolder& other) const;

  // Returns the wrapped country ID, usable in test code only.
  CountryId GetForTesting() const;

  // See `GetRestricted(CountryAccessReason)`.
  CountryId GetRestricted(
      CountryAccessKey<TemplateURLPrepopulateData::Resolver>) const;
  CountryId GetRestricted(
      CountryAccessKey<search_engines::SearchEngineChoiceService>) const;
  CountryId GetRestricted(CountryAccessKey<RegionalCapabilitiesService>) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(RegionalCapabilitiesCountryIdTest, GetRestricted);

  // Returns the wrapped country ID.
  //
  // Access is restricted (see crbug.com/328040066 for context). To get access,
  // please declare a new `CountryAccessReason` enum value, set up the access
  // key methods, link a crbug with context on the approval
  // (go/regional-capabilities-country-access-request, Google-internal only,
  // sorry) and add the caller BUILD target in
  // `//c/regional_capabilities:country_access_reason`'s visibility list.
  CountryId GetRestricted(CountryAccessReason) const;

  CountryId country_id_;
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_COUNTRY_ID_H_
