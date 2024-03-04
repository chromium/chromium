// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/disambiguate_possible_field_types.h"

#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using test::CreateTestFormField;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

// Tests that `DisambiguatePossibleFieldTypes` makes the correct choices.
class DisambiguatePossibleFieldTypesTest : public ::testing::Test {
 protected:
  struct TestFieldData {
    FieldType predicted_type;
    FieldTypeSet ambiguous_possible_field_types;
  };

  std::vector<FieldTypeSet> GetDisambiguatedPossibleFieldTypes(
      const std::vector<TestFieldData>& test_fields) {
    FormData form;
    for (size_t i = 0; i < test_fields.size(); ++i) {
      form.fields.push_back(
          CreateTestFormField("", "", "", FormControlType::kInputText));
    }
    FormStructure form_structure(form);
    for (size_t i = 0; i < test_fields.size(); ++i) {
      form_structure.field(i)->set_possible_types(
          test_fields[i].ambiguous_possible_field_types);
      form_structure.field(i)->set_server_predictions(
          {::autofill::test::CreateFieldPrediction(
              test_fields[i].predicted_type)});
    }

    DisambiguatePossibleFieldTypes(&form_structure);

    std::vector<FieldTypeSet> disambiguated_possible_field_types;
    base::ranges::transform(
        form_structure.fields(),
        std::back_inserter(disambiguated_possible_field_types),
        [](const std::unique_ptr<AutofillField>& field) {
          return field->possible_types();
        });
    return disambiguated_possible_field_types;
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Name disambiguation.
// An ambiguous name field that has no next field and that is preceded by a non
// credit card field should be disambiguated as a non credit card name.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNamePrecededByAddressDisambiguatesToAddress) {
  std::vector<TestFieldData> test_fields = {
      {ADDRESS_HOME_CITY, {ADDRESS_HOME_CITY}},
      {CREDIT_CARD_NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {CREDIT_CARD_NAME_LAST,
       {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(disambiguated_possible_field_types,
              ElementsAre(UnorderedElementsAre(ADDRESS_HOME_CITY),
                          UnorderedElementsAre(NAME_FIRST),
                          UnorderedElementsAre(NAME_LAST, NAME_LAST_SECOND)));
}

// An ambiguous name field that has no next field and that is preceded by a
// credit card field should be disambiguated as a credit card name.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNamePrecededByCreditCardDisambiguatesToCreditCard) {
  std::vector<TestFieldData> test_fields = {
      {CREDIT_CARD_NUMBER, {CREDIT_CARD_NUMBER}},
      {CREDIT_CARD_NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {CREDIT_CARD_NAME_LAST,
       {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(disambiguated_possible_field_types,
              ElementsAre(UnorderedElementsAre(CREDIT_CARD_NUMBER),
                          UnorderedElementsAre(CREDIT_CARD_NAME_FIRST),
                          UnorderedElementsAre(CREDIT_CARD_NAME_LAST)));
}

// An ambiguous name field that has no previous field and that is followed by an
// address field should be disambiguated as an address name.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNameFollowedByAddressDisambiguatesToAddress) {
  std::vector<TestFieldData> test_fields = {
      {CREDIT_CARD_NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {CREDIT_CARD_NAME_LAST,
       {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}},
      {ADDRESS_HOME_CITY, {ADDRESS_HOME_CITY}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(disambiguated_possible_field_types,
              ElementsAre(UnorderedElementsAre(NAME_FIRST),
                          UnorderedElementsAre(NAME_LAST, NAME_LAST_SECOND),
                          UnorderedElementsAre(ADDRESS_HOME_CITY)));
}

// An ambiguous name field that has no previous field and that is followed by a
// credit card field should be disambiguated as a credit card name.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNameFollowedByCreditCardDisambiguatesToCreditCard) {
  std::vector<TestFieldData> test_fields = {
      {NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {NAME_LAST, {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}},
      {CREDIT_CARD_NUMBER, {CREDIT_CARD_NUMBER}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(disambiguated_possible_field_types,
              ElementsAre(UnorderedElementsAre(CREDIT_CARD_NAME_FIRST),
                          UnorderedElementsAre(CREDIT_CARD_NAME_LAST),
                          UnorderedElementsAre(CREDIT_CARD_NUMBER)));
}

// An ambiguous name field that is preceded and followed by non credit card
// fields should be disambiguated as a non credit card name.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNameSurroundedByAddressDisambiguatesToAddress) {
  std::vector<TestFieldData> test_fields = {
      {ADDRESS_HOME_CITY, {ADDRESS_HOME_CITY}},
      {CREDIT_CARD_NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {CREDIT_CARD_NAME_LAST,
       {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}},
      {ADDRESS_HOME_STATE, {ADDRESS_HOME_STATE}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(disambiguated_possible_field_types,
              ElementsAre(UnorderedElementsAre(ADDRESS_HOME_CITY),
                          UnorderedElementsAre(NAME_FIRST),
                          UnorderedElementsAre(NAME_LAST, NAME_LAST_SECOND),
                          UnorderedElementsAre(ADDRESS_HOME_STATE)));
}

// An ambiguous name field that is preceded and followed by credit card fields
// should be disambiguated as a credit card name.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNameSurroundedByCreditCardDisambiguatesToCreditCard) {
  std::vector<TestFieldData> test_fields = {
      {CREDIT_CARD_NUMBER, {CREDIT_CARD_NUMBER}},
      {NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {NAME_LAST, {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR, {CREDIT_CARD_EXP_4_DIGIT_YEAR}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(disambiguated_possible_field_types,
              ElementsAre(UnorderedElementsAre(CREDIT_CARD_NUMBER),
                          UnorderedElementsAre(CREDIT_CARD_NAME_FIRST),
                          UnorderedElementsAre(CREDIT_CARD_NAME_LAST),
                          UnorderedElementsAre(CREDIT_CARD_EXP_4_DIGIT_YEAR)));
}

// An ambiguous name field that is preceded by a non credit card field and
// followed by a credit card field should not be disambiguated.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNamePrecededByAddressFollowedByCreditCardRemainsAmbiguous) {
  std::vector<TestFieldData> test_fields = {
      {ADDRESS_HOME_CITY, {ADDRESS_HOME_CITY}},
      {NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {NAME_LAST, {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR, {CREDIT_CARD_EXP_4_DIGIT_YEAR}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(
      disambiguated_possible_field_types,
      ElementsAre(UnorderedElementsAre(ADDRESS_HOME_CITY),
                  UnorderedElementsAre(NAME_FIRST, CREDIT_CARD_NAME_FIRST),
                  UnorderedElementsAre(NAME_LAST, CREDIT_CARD_NAME_LAST,
                                       NAME_LAST_SECOND),
                  UnorderedElementsAre(CREDIT_CARD_EXP_4_DIGIT_YEAR)));
}

// An ambiguous name field that is preceded by a credit card field and followed
// by a non credit card field should not be disambiguated.
TEST_F(DisambiguatePossibleFieldTypesTest,
       AmbiguousNamePrecededByCreditCardFollowedByAddressRemainsAmbiguous) {
  std::vector<TestFieldData> test_fields = {
      {CREDIT_CARD_EXP_4_DIGIT_YEAR, {CREDIT_CARD_EXP_4_DIGIT_YEAR}},
      {NAME_FIRST, {NAME_FIRST, CREDIT_CARD_NAME_FIRST}},
      {NAME_LAST, {NAME_LAST, CREDIT_CARD_NAME_LAST, NAME_LAST_SECOND}},
      {ADDRESS_HOME_CITY, {ADDRESS_HOME_CITY}}};

  std::vector<FieldTypeSet> disambiguated_possible_field_types =
      GetDisambiguatedPossibleFieldTypes(test_fields);

  EXPECT_THAT(
      disambiguated_possible_field_types,
      ElementsAre(UnorderedElementsAre(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                  UnorderedElementsAre(NAME_FIRST, CREDIT_CARD_NAME_FIRST),
                  UnorderedElementsAre(NAME_LAST, CREDIT_CARD_NAME_LAST,
                                       NAME_LAST_SECOND),
                  UnorderedElementsAre(ADDRESS_HOME_CITY)));
}

}  // namespace autofill
