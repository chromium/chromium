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
  // Test `FieldTypeGroup::kName` fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(
      GetFieldTypesOfGroup(FieldTypeGroup::kName)));
  // Test `FieldTypeGroup::kPhone` fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(
      GetFieldTypesOfGroup(FieldTypeGroup::kPhone)));
  // Test `FieldTypeGroup::kEmail` fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(
      GetFieldTypesOfGroup(FieldTypeGroup::kEmail)));
  // Tests address fields, which in the context of granular filling
  // are both `FieldTypeGroup::kAddress` and `FieldTypeGroup::kCompany` fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(GetAddressFieldsForGroupFilling()));
}

// Granular filling address group also considers company to be an address field.
// Therefore, a group containing ONLY address fields, should not be considered
// a granular filling group.
TEST(AutofillGranularFillingUtilsTest,
     AreFieldsGranularFillingGroup_ReturnsFalseForAutofillAddressFieldsOnly) {
  EXPECT_FALSE(AreFieldsGranularFillingGroup(
      GetFieldTypesOfGroup(FieldTypeGroup::kAddress)));
}

}  // namespace autofill
