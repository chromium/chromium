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

TEST(
    AutofillGranularFillingUtilsTest,
    GetTargetServerFieldsForTypeAndLastTargetedFields_SingleField_ReturnsTriggeringFieldType) {
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                {ADDRESS_HOME_LINE1}, AutofillType(NAME_FIRST)),
            ServerFieldTypeSet({NAME_FIRST}));
}

TEST(
    AutofillGranularFillingUtilsTest,
    GetTargetServerFieldsForTypeAndLastTargetedFields_AddressFieldsGroup_ReturnsTriggeringFieldTypeGroup) {
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                GetAddressFieldsForGroupFilling(), AutofillType(NAME_FIRST)),
            GetServerFieldTypesOfGroup(FieldTypeGroup::kName));
}

TEST(
    AutofillGranularFillingUtilsTest,
    GetTargetServerFieldsForTypeAndLastTargetedFields_FieldsMatchGroupFillingButTargetFieldDoesnot_ReturnsAllServerTypes) {
  // If the previously targeted fields match a group of fields (such as name or
  // address), but the triggering field is not a field for which we offer group
  // filling (such as CREDIT_CARD_NAME_FULL), we default back to full form
  // filling.
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                GetAddressFieldsForGroupFilling(),
                AutofillType(CREDIT_CARD_NAME_FULL)),
            kAllServerFieldTypes);
}

TEST(
    AutofillGranularFillingUtilsTest,
    GetTargetServerFieldsForTypeAndLastTargetedFields_AllServerTypes_ReturnsAllServerTypes) {
  // Regardless of the triggering field, if the last targeted fields were
  // kAllServerFieldTypes, i.e full form, we will also return
  // kAllServerFieldTypes, so that the user stays in the full form granularity
  // level.
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                kAllServerFieldTypes, AutofillType(NAME_FIRST)),
            kAllServerFieldTypes);
}

}  // namespace autofill
