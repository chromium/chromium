// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_type_utils.h"

#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  EXPECT_EQ(NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kAddress),
            0U);

  EXPECT_EQ(NumberOfPossibleFieldTypesInGroup(field, FieldTypeGroup::kPhone),
            0U);
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

TEST(AutofillFieldTypeUtils, AddressLineIndexTest) {
  EXPECT_THAT(AddressLineIndex(ADDRESS_HOME_LINE1), 0);
  EXPECT_THAT(AddressLineIndex(ADDRESS_HOME_LINE2), 1);
  EXPECT_THAT(AddressLineIndex(ADDRESS_HOME_LINE3), 2);
}

}  // namespace

}  // namespace autofill
