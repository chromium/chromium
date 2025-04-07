// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_country_id.h"

#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace regional_capabilities {

TEST(RegionalCapabilitiesCountryIdTest, EqualityChecks) {
  CountryIdHolder us_holder(country_codes::CountryId("US"));
  CountryIdHolder other_us_holder(country_codes::CountryId("US"));
  CountryIdHolder fr_holder(country_codes::CountryId("FR"));
  CountryIdHolder unknown_holder((country_codes::CountryId()));

  EXPECT_EQ(us_holder, other_us_holder);
  EXPECT_NE(us_holder, fr_holder);
  EXPECT_NE(us_holder, unknown_holder);
}

TEST(RegionalCapabilitiesCountryIdTest, GetRestricted) {
  auto country_id = country_codes::CountryId("US");

  CountryIdHolder country_id_holder(country_id);

  auto actual_country_id = country_id_holder.GetRestricted(
      CountryAccessKey(CountryAccessReason::kProfileInternalsDisplayInDebugUi));

  EXPECT_EQ(actual_country_id, country_id);
}

}  // namespace regional_capabilities
