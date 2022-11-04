// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "components/autofill/core/browser/field_type_utils.h"

namespace autofill {

namespace {

TEST(AutofillFieldTypeUtils, NumberOfPossibleTypesInGroup) {
  autofill::AutofillField field;
  field.set_possible_types({NAME_FIRST, NAME_LAST, CREDIT_CARD_NAME_FIRST});

  EXPECT_EQ(NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kName),
            2U);

  EXPECT_EQ(
      NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kCreditCard),
      1U);

  EXPECT_EQ(
      NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kAddressHome),
      0U);

  EXPECT_EQ(
      NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kPhoneHome), 0U);
}

TEST(AutofillFieldTypeUtils, FieldHasMeaningfulFieldTypes) {
  autofill::AutofillField field;

  // Test that a meaningful type correctly detected.
  field.set_possible_types({NAME_FIRST, NAME_LAST, CREDIT_CARD_NAME_FIRST});
  EXPECT_TRUE(FieldHasMeaningfulPossibleFieldTypes(field));

  // Test that field with only meaningless types are correctly detected.
  field.set_possible_types({EMPTY_TYPE});
  EXPECT_FALSE(FieldHasMeaningfulPossibleFieldTypes(field));

  field.set_possible_types({UNKNOWN_TYPE});
  EXPECT_FALSE(FieldHasMeaningfulPossibleFieldTypes(field));
}

}  // namespace

}  // namespace autofill
