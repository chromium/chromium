// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_INTERNALS_DATA_HOLDER_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_INTERNALS_DATA_HOLDER_H_

#include "base/containers/flat_map.h"

namespace regional_capabilities {

class CountryAccessKey;
class RegionalCapabilitiesService;

enum class CountryAccessReason;

class InternalsDataHolder final {
 public:
  explicit InternalsDataHolder(
      RegionalCapabilitiesService& regional_capabilities);

  InternalsDataHolder(const InternalsDataHolder& other);
  InternalsDataHolder& operator=(const InternalsDataHolder& other);

  ~InternalsDataHolder();

  bool operator==(const InternalsDataHolder& other) const;

  // Returns the wrapped country ID, usable in test code only.
  const base::flat_map<std::string, std::string>& GetForTesting() const;

  // Returns the wrapped internals data.
  //
  // Access is restricted (see crbug.com/328040066 for context). To get access,
  // please declare a new `CountryAccessReason` enum value, set up the access
  // key methods, link a crbug with context on the approval
  // (go/regional-capabilities-country-access-request, Google-internal only,
  // sorry) and add the caller BUILD target in
  // `//c/regional_capabilities:country_access_reason`'s visibility list.
  const base::flat_map<std::string, std::string>& GetRestricted(
      CountryAccessKey) const;

 private:
  base::flat_map<std::string, std::string> data_;
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_INTERNALS_DATA_HOLDER_H_
