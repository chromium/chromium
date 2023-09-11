// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_granular_filling_utils.h"

#include "components/autofill/core/browser/autofill_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(
    AutofillGranularFillingUtilsTest,
    AreFieldsGranularFillingGroup_ReturnsTrueWhenFieldsMatchAGroupFillingGroup) {
  // Test name fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(
      GetServerFieldTypesOfGroup(FieldTypeGroup::kName)));
  // Test phone fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(
      GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone)));
  // Tests address fields, which in the context of granular filling
  // are both `FieldTypeGroup::kName` and `FieldTypeGroup::kCompany` fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(GetAddressFieldsForGroupFilling()));
}

// Granular filling address group also considers company to be an address field.
// Therefore, a group containing ONLY address fields, should not be considered
// a granular filling group.
TEST(AutofillGranularFillingUtilsTest,
     AreFieldsGranularFillingGroup_ReturnsFalseForAutofillAddressFieldsOnly) {
  EXPECT_FALSE(AreFieldsGranularFillingGroup(
      GetServerFieldTypesOfGroup(FieldTypeGroup::kAddress)));
}

}  // namespace autofill
