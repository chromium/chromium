// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_

#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses::test {

// Used in testing the GetOrCreate, Reserve, and Create network requests.
std::string MakeCreationResponse(const PlusProfile& profile);
// Used in testing the List network requests.
std::string MakeListResponse(const std::vector<PlusProfile>& profiles);
// Converts a PlusProfile to an equivalent JSON string.
std::string MakePlusProfile(const PlusProfile& profile);

}  // namespace plus_addresses::test

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TEST_UTILS_H_
