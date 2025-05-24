// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_utils.h"

#include "components/country_codes/country_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace regional_capabilities {

using ::country_codes::CountryId;

// Sanity check the list.
TEST(RegionalCapabilitiesUtilsTest, IsEeaCountry) {
  EXPECT_TRUE(IsEeaCountry(CountryId("DE")));
  EXPECT_TRUE(IsEeaCountry(CountryId("FR")));
  EXPECT_TRUE(IsEeaCountry(CountryId("VA")));
  EXPECT_TRUE(IsEeaCountry(CountryId("AX")));
  EXPECT_TRUE(IsEeaCountry(CountryId("YT")));
  EXPECT_TRUE(IsEeaCountry(CountryId("NC")));

  EXPECT_FALSE(IsEeaCountry(CountryId("US")));
  EXPECT_FALSE(IsEeaCountry(CountryId("JP")));
  EXPECT_FALSE(IsEeaCountry(CountryId()));
}

}  // namespace regional_capabilities
