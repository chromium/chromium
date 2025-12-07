// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_COUNTRY_ID_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_COUNTRY_ID_H_

#include "components/country_codes/country_codes.h"

namespace regional_capabilities {

class CountryAccessKey;

enum class CountryAccessReason;

class CountryIdHolder final {
 public:
  explicit CountryIdHolder(country_codes::CountryId country_id);

  CountryIdHolder(const CountryIdHolder& other);
  CountryIdHolder& operator=(const CountryIdHolder& other);

  ~CountryIdHolder();

  bool operator==(const CountryIdHolder& other) const;

  // Returns the wrapped country ID, usable in test code only.
  country_codes::CountryId GetForTesting() const;

  // Returns the wrapped country ID.
  //
  // Access is restricted (see crbug.com/328040066 for context). To get access,
  // please declare a new `CountryAccessReason` enum value, set up the access
  // key methods, link a crbug with context on the approval
  // (go/regional-capabilities-country-access-request, Google-internal only,
  // sorry) and add the caller BUILD target in
  // `//c/regional_capabilities:country_access_reason`'s visibility list.
  country_codes::CountryId GetRestricted(CountryAccessKey) const;

 private:
  country_codes::CountryId country_id_;
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_COUNTRY_ID_H_
