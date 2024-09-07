// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_requirement_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(ProfileRequirementUtilsTest, IsMinimumAddress) {
  AutofillProfile profile = test::GetFullProfile();
  EXPECT_TRUE(IsMinimumAddress(profile));
}

TEST(ProfileRequirementUtilsTest, IncompleteAddress) {
  AutofillProfile profile = test::GetFullProfile();
  profile.ClearFields({ADDRESS_HOME_ZIP});
  EXPECT_FALSE(IsMinimumAddress(profile));
}

// Mexico is special in that it has cities and municipios/demarcaciones
// territoriales. Some websites ask for only one of the two and we still want to
// offer saving addresses.
TEST(ProfileRequirementUtilsTest, MXAddress) {
  auto generate_address_stub = []() {
    AutofillProfile profile(AddressCountryCode("MX"));
    profile.SetRawInfo(ADDRESS_HOME_LINE1, u"C. Montes Urales 445, Lomas");
    profile.SetRawInfo(ADDRESS_HOME_ZIP, u"11000");
    profile.SetRawInfo(ADDRESS_HOME_STATE, u"CDMX");
    profile.FinalizeAfterImport();
    return profile;
  };

  EXPECT_FALSE(IsMinimumAddress(generate_address_stub()));

  AutofillProfile profile_with_city = generate_address_stub();
  profile_with_city.SetRawInfo(ADDRESS_HOME_CITY, u"Ciudad de México");
  profile_with_city.FinalizeAfterImport();
  EXPECT_TRUE(IsMinimumAddress(profile_with_city));

  AutofillProfile profile_with_municipality = generate_address_stub();
  profile_with_municipality.SetRawInfo(ADDRESS_HOME_ADMIN_LEVEL2,
                                       u"Miguel Hidalgo");
  profile_with_municipality.FinalizeAfterImport();
  EXPECT_TRUE(IsMinimumAddress(profile_with_municipality));
}

}  // namespace autofill
