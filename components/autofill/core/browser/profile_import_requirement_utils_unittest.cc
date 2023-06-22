// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_import_requirement_utils.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(ProfileImportRequirementUtilsTest, IsMinimumAddress) {
  AutofillProfile profile = test::GetFullProfile();
  EXPECT_TRUE(IsMinimumAddress(profile, "US", "en", nullptr, false));
}

TEST(ProfileImportRequirementUtilsTest, IncompleteAddress) {
  AutofillProfile profile = test::GetFullProfile();
  profile.ClearFields({ADDRESS_HOME_ZIP});
  EXPECT_FALSE(IsMinimumAddress(profile, "US", "en", nullptr, false));
}

}  // namespace autofill
