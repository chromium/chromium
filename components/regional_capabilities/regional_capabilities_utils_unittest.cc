// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_utils.h"

#include "components/country_codes/country_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace regional_capabilities {

// Sanity check the list.
TEST(RegionalCapabilitiesUtilsTest, IsEeaCountry) {
  using country_codes::CountryCharsToCountryID;

  EXPECT_TRUE(IsEeaCountry(CountryCharsToCountryID('D', 'E')));
  EXPECT_TRUE(IsEeaCountry(CountryCharsToCountryID('F', 'R')));
  EXPECT_TRUE(IsEeaCountry(CountryCharsToCountryID('V', 'A')));
  EXPECT_TRUE(IsEeaCountry(CountryCharsToCountryID('A', 'X')));
  EXPECT_TRUE(IsEeaCountry(CountryCharsToCountryID('Y', 'T')));
  EXPECT_TRUE(IsEeaCountry(CountryCharsToCountryID('N', 'C')));

  EXPECT_FALSE(IsEeaCountry(CountryCharsToCountryID('U', 'S')));
  EXPECT_FALSE(IsEeaCountry(CountryCharsToCountryID('J', 'P')));
  EXPECT_FALSE(IsEeaCountry(country_codes::kCountryIDUnknown));
}

}  // namespace regional_capabilities
