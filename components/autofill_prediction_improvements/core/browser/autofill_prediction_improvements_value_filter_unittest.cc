// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_value_filter.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {

namespace {

using autofill::AutofillField;
using autofill::FieldType;
using autofill::FieldTypeSet;
using autofill::FormData;
using autofill::FormStructure;
using autofill::FormStructureTestApi;

bool is_filtered(const AutofillField& field) {
  return field.value_identified_as_potentially_sensitive();
}

struct FieldFilterTestCase {
  // The test expectation if the field in the form should have been filtered.
  bool should_be_filtered;
  // The type of the field as it was detected by Autofill.
  FieldType field_type = autofill::UNKNOWN_TYPE;
  // The possible type of the value as it was determined by comparing the value
  // to all known data.
  FieldType possible_value_type = autofill::UNKNOWN_TYPE;
  // The type of the value that was autofilled with a fallback mechanism.
  // UNKNOWN_TYPE indicates that it was not filled.
  FieldType autofilled_type = autofill::UNKNOWN_TYPE;
  // Indicates if the field is actually a password field.
  bool is_password_field = false;
};

class PredictionImprovementsFieldFilterTest
    : public testing::TestWithParam<FieldFilterTestCase> {};

TEST_P(PredictionImprovementsFieldFilterTest, TestFilter) {
  FieldFilterTestCase test_case = GetParam();

  // Creates a test form with a single field.
  autofill::FormStructure form{FormData()};
  FormStructureTestApi form_test_api{form};
  AutofillField& field = form_test_api.PushField();

  // Use the heuristic type prediction to assign a type to the field.
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  field.set_heuristic_type(autofill::HeuristicSource::kDefaultRegexes,
                           test_case.field_type);
#else
  field.set_heuristic_type(autofill::HeuristicSource::kLegacyRegexes,
                           test_case.field_type);
#endif

  // Set the possible type that was determined from the value.
  field.set_possible_types(FieldTypeSet{test_case.possible_value_type});

  // Set the autofilled type. UNKNOWN_TYPE indicates that the field was not
  // used.
  if (test_case.autofilled_type != autofill::UNKNOWN_TYPE) {
    field.set_autofilled_type(test_case.autofilled_type);
  }

  // Change the input element if the field was a password field.
  if (test_case.is_password_field) {
    field.set_form_control_type(autofill::FormControlType::kInputPassword);
  }

  // Apply the actual filter logic and check if the field was filtered.
  FilterSensitiveValues(form);
  EXPECT_EQ(is_filtered(field), test_case.should_be_filtered);
}

INSTANTIATE_TEST_SUITE_P(
    FilterByInputElementTypeTest,
    PredictionImprovementsFieldFilterTest,
    testing::Values(FieldFilterTestCase{.should_be_filtered = false,
                                        .is_password_field = false},
                    FieldFilterTestCase{.should_be_filtered = true,
                                        .is_password_field = true}));

INSTANTIATE_TEST_SUITE_P(
    FilterByFieldTypeTest,
    PredictionImprovementsFieldFilterTest,
    testing::Values(FieldFilterTestCase{.should_be_filtered = false,
                                        .field_type = autofill::UNKNOWN_TYPE},
                    FieldFilterTestCase{.should_be_filtered = false,
                                        .field_type = autofill::NAME_FULL},
                    FieldFilterTestCase{.should_be_filtered = true,
                                        .field_type = autofill::USERNAME},
                    FieldFilterTestCase{.should_be_filtered = true,
                                        .field_type = autofill::PASSWORD},
                    FieldFilterTestCase{
                        .should_be_filtered = true,
                        .field_type = autofill::CREDIT_CARD_NUMBER}));

INSTANTIATE_TEST_SUITE_P(
    FilterByFilledTypeTest,
    PredictionImprovementsFieldFilterTest,
    testing::Values(
        FieldFilterTestCase{.should_be_filtered = false,
                            .autofilled_type = autofill::UNKNOWN_TYPE},
        FieldFilterTestCase{.should_be_filtered = false,
                            .autofilled_type = autofill::NAME_FULL},
        FieldFilterTestCase{.should_be_filtered = true,
                            .autofilled_type = autofill::USERNAME},
        FieldFilterTestCase{.should_be_filtered = true,
                            .autofilled_type = autofill::PASSWORD},
        FieldFilterTestCase{.should_be_filtered = true,
                            .autofilled_type = autofill::CREDIT_CARD_NUMBER}));

INSTANTIATE_TEST_SUITE_P(
    FilterByPossibleValueTest,
    PredictionImprovementsFieldFilterTest,
    testing::Values(
        FieldFilterTestCase{.should_be_filtered = false,
                            .possible_value_type = autofill::UNKNOWN_TYPE},
        FieldFilterTestCase{.should_be_filtered = false,
                            .possible_value_type = autofill::NAME_FULL},
        FieldFilterTestCase{.should_be_filtered = false,
                            .possible_value_type = autofill::USERNAME},
        FieldFilterTestCase{.should_be_filtered = true,
                            .possible_value_type = autofill::PASSWORD},
        FieldFilterTestCase{
            .should_be_filtered = false,
            .possible_value_type = autofill::CREDIT_CARD_NAME_FULL},
        FieldFilterTestCase{
            .should_be_filtered = true,
            .possible_value_type = autofill::CREDIT_CARD_NUMBER}));

}  // namespace

}  // namespace autofill_prediction_improvements
