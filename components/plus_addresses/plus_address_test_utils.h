// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_

#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plus_addresses::test {

// Returns a fully populated, confirmed PlusProfile. If `use_full_domain` is
// true, a full domain (as opposed to eTLD+1) is used as facet.
// TODO(b/322147254): Remove parameter once plus addresses starts fully relying
// on sync data.
PlusProfile CreatePlusProfile(bool use_full_domain = false);
// Returns a fully populated, confirmed PlusProfile different from
// `CreatePlusProfile()`. If `use_full_domain` is true, a full domain (as
// opposed to eTLD+1) is used as facet.
// TODO(b/322147254): Remove parameter once plus addresses starts fully relying
// on sync data.
PlusProfile CreatePlusProfile2(bool use_full_domain = false);

// Returns a fully populated, confirmed PlusProfile with the given `facet`.
PlusProfile CreatePlusProfileWithFacet(const affiliations::FacetURI& facet);

// Used in testing the GetOrCreate, Reserve, and Create network requests.
std::string MakeCreationResponse(const PlusProfile& profile);
// Used in testing the List network requests.
std::string MakeListResponse(const std::vector<PlusProfile>& profiles);
// Converts a PlusProfile to an equivalent JSON string.
std::string MakePlusProfile(const PlusProfile& profile);

}  // namespace plus_addresses::test

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
