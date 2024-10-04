// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_utils.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {

namespace {

using autofill::AutofillField;
using autofill::FieldType;
using autofill::FormData;
using autofill::FormStructure;
using autofill::FormStructureTestApi;
using autofill::GetActiveHeuristicSource;
using autofill::HeuristicSource;

void AddHeuristicType(AutofillField& field, autofill::FieldType type) {
  field.set_heuristic_type(GetActiveHeuristicSource(), type);
}

void AddImprovedPredictionType(AutofillField& field) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  field.set_heuristic_type(HeuristicSource::kPredictionImprovementRegexes,
                           autofill::IMPROVED_PREDICTION);
#else
  AddHeuristicType(field, autofill::IMPROVED_PREDICTION);
#endif
}

struct FieldEligibilityByTypeTestCase {
  bool should_be_eligible;
  bool has_prediction_improvement_type = false;
  FieldType autofill_type = autofill::UNKNOWN_TYPE;
};

class PredictionImprovementsFieldEligibilityByTypeTest
    : public testing::TestWithParam<FieldEligibilityByTypeTestCase> {};

TEST_P(PredictionImprovementsFieldEligibilityByTypeTest,
       FieldEligibilityByType) {
  FieldEligibilityByTypeTestCase test_case = GetParam();

  AutofillField field;
  if (test_case.has_prediction_improvement_type) {
    AddImprovedPredictionType(field);
  }
  if (test_case.autofill_type != autofill::UNKNOWN_TYPE) {
    AddHeuristicType(field, test_case.autofill_type);
  }

  EXPECT_EQ(IsFieldEligibleByTypeCriteria(field), test_case.should_be_eligible);
}

INSTANTIATE_TEST_SUITE_P(
    FieldEligibilityByTypeTest,
    PredictionImprovementsFieldEligibilityByTypeTest,
    testing::Values(
        FieldEligibilityByTypeTestCase{.should_be_eligible = true,
                                       .has_prediction_improvement_type = true,
                                       .autofill_type = autofill::UNKNOWN_TYPE},
        FieldEligibilityByTypeTestCase{
            .should_be_eligible = true,
            .has_prediction_improvement_type = true,
            .autofill_type = autofill::ADDRESS_HOME_LINE1},
        FieldEligibilityByTypeTestCase{
            .should_be_eligible = false,
            .has_prediction_improvement_type = true,
            .autofill_type = autofill::CREDIT_CARD_NUMBER},
        FieldEligibilityByTypeTestCase{
            .should_be_eligible = false,
            .has_prediction_improvement_type = false,
            .autofill_type = autofill::CREDIT_CARD_NUMBER},
        FieldEligibilityByTypeTestCase{.should_be_eligible = false,
                                       .has_prediction_improvement_type = false,
                                       .autofill_type = autofill::UNKNOWN_TYPE},
        FieldEligibilityByTypeTestCase{
            .should_be_eligible = true,
            .has_prediction_improvement_type = false,
            .autofill_type = autofill::ADDRESS_HOME_LINE1}));

// Test that an empty form is not eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleForFillingByFieldTypeCriteria_EmptyForm) {
  FormData form_data;
  FormStructure form(form_data);
  FormStructureTestApi form_test_api(form);

  EXPECT_FALSE(IsFormEligibleForFillingByFieldCriteria(form));
}

// Test that a form with a single UNKNOWN_TYPE field is not eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleForFillingByFieldTypeCriteria_SingleUnknownField) {
  FormData form_data;
  FormStructure form(form_data);
  FormStructureTestApi form_test_api(form);

  AutofillField& unknown_field = form_test_api.PushField();
  unknown_field.set_heuristic_type(GetActiveHeuristicSource(),
                                   autofill::UNKNOWN_TYPE);
}

// Test that a form with a single address field is not eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleForFillingByFieldTypeCriteria_SingleAddressField) {
  FormData form_data;
  FormStructure form(form_data);
  FormStructureTestApi form_test_api(form);

  // Add an additional address field and verify that is also does not yield and
  // overall eligible form.
  AutofillField& address_field = form_test_api.PushField();
  address_field.set_heuristic_type(GetActiveHeuristicSource(),
                                   autofill::NAME_FIRST);
  EXPECT_TRUE(IsFormEligibleForFillingByFieldCriteria(form));
}

// Test that a form with an eligible field is overall eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleForFillingByFieldTypeCriteria_SingleEligibleField) {
  FormData form_data;
  FormStructure form(form_data);
  FormStructureTestApi form_test_api(form);

  AutofillField& prediction_improvement_field = form_test_api.PushField();
  AddImprovedPredictionType(prediction_improvement_field);
  EXPECT_TRUE(IsFormEligibleForFillingByFieldCriteria(form));
}

// Test that a form with an eligible but unfocusable field is not eligible.
TEST(
    AutofillPredictionImprovementsUtilsTest,
    IsFormEligibleForFillingByFieldTypeCriteria_SingleUnfocusableEligibleField) {
  FormData form_data;
  FormStructure form(form_data);
  FormStructureTestApi form_test_api(form);

  AutofillField& prediction_improvement_field = form_test_api.PushField();
  AddImprovedPredictionType(prediction_improvement_field);
  prediction_improvement_field.set_is_focusable(false);
  EXPECT_FALSE(IsFormEligibleForFillingByFieldCriteria(form));
}

// Test that a form with an eligible field is overall eligible.
TEST(
    AutofillPredictionImprovementsUtilsTest,
    IsFormEligibleForFillingByFieldTypeCriteria_SingleEligibleField_Prefilled) {
  FormData form_data;
  FormStructure form(form_data);
  FormStructureTestApi form_test_api(form);

  AutofillField& prediction_improvement_field = form_test_api.PushField();
  AddImprovedPredictionType(prediction_improvement_field);
  prediction_improvement_field.set_value(u"prefilled_value");

  EXPECT_FALSE(IsFormEligibleForFillingByFieldCriteria(form));
}

// Test that a form with an eligible field is overall eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleForFillingByFieldTypeCriteria_MixedFormWithEligibleField) {
  FormData form_data;
  FormStructure form(form_data);
  FormStructureTestApi form_test_api(form);

  AutofillField& unknown_field = form_test_api.PushField();
  unknown_field.set_heuristic_type(GetActiveHeuristicSource(),
                                   autofill::UNKNOWN_TYPE);

  AutofillField& address_field = form_test_api.PushField();
  address_field.set_heuristic_type(GetActiveHeuristicSource(),
                                   autofill::NAME_FIRST);

  AutofillField& prediction_improvement_field = form_test_api.PushField();
  AddImprovedPredictionType(prediction_improvement_field);
  EXPECT_TRUE(IsFormEligibleForFillingByFieldCriteria(form));
}

}  // namespace

}  // namespace autofill_prediction_improvements
