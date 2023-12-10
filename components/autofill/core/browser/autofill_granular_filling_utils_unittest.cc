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
      GetServerFieldTypesOfGroup(FieldTypeGroup::kName)));
  // Test `FieldTypeGroup::kPhone` fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(
      GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone)));
  // Test `FieldTypeGroup::kEmail` fields.
  EXPECT_TRUE(AreFieldsGranularFillingGroup(
      GetServerFieldTypesOfGroup(FieldTypeGroup::kEmail)));
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
      GetServerFieldTypesOfGroup(FieldTypeGroup::kAddress)));
}

TEST(
    AutofillGranularFillingUtilsTest,
    GetTargetServerFieldsForTypeAndLastTargetedFields_SingleField_ReturnsTriggeringFieldType) {
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                {ADDRESS_HOME_LINE1}, NAME_FIRST),
            ServerFieldTypeSet({NAME_FIRST}));
}

// The test below asserts that when the last targeted fields match
// `AutofillFillingMethod::kGroupFilling`,
// `GetTargetServerFieldsForTypeAndLastTargetedFields()` returns a set of fields
// that match the group of the triggering field.
TEST(
    AutofillGranularFillingUtilsTest,
    GetTargetServerFieldsForTypeAndLastTargetedFields_GroupFilling_ReturnsTriggeringFieldTypeGroup) {
  //`FieldTypeGroup::kName` triggering field.
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                GetAddressFieldsForGroupFilling(),
                /*triggering_field_type=*/NAME_FIRST),
            GetServerFieldTypesOfGroup(FieldTypeGroup::kName));

  //`FieldTypeGroup::kCompany` triggering field.
  // Note that `FieldTypeGroup::kCompany` behaves the same as
  // `FieldTypeGroup::kAddress`.
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                GetServerFieldTypesOfGroup(FieldTypeGroup::kName),
                /*triggering_field_type=*/COMPANY_NAME),
            GetAddressFieldsForGroupFilling());

  //`FieldTypeGroup::kAddress` triggering field.
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                GetServerFieldTypesOfGroup(FieldTypeGroup::kName),
                /*triggering_field_type=*/ADDRESS_HOME_LINE1),
            GetAddressFieldsForGroupFilling());

  //`FieldTypeGroup::kEmail` triggering field.
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                GetAddressFieldsForGroupFilling(),
                /*triggering_field_type=*/EMAIL_ADDRESS),
            GetServerFieldTypesOfGroup(FieldTypeGroup::kEmail));
}

TEST(
    AutofillGranularFillingUtilsTest,
    GetTargetServerFieldsForTypeAndLastTargetedFields_FieldsMatchGroupFillingButTargetFieldDoesNot_ReturnsAllServerTypes) {
  // If the previously targeted fields match a group of fields (such as name or
  // address), but the triggering field is not a field for which we offer group
  // filling (such as CREDIT_CARD_NAME_FULL), we default back to full form
  // filling.
  EXPECT_EQ(GetTargetServerFieldsForTypeAndLastTargetedFields(
                GetAddressFieldsForGroupFilling(), CREDIT_CARD_NAME_FULL),
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
                kAllServerFieldTypes, NAME_FIRST),
            kAllServerFieldTypes);
}

}  // namespace autofill
