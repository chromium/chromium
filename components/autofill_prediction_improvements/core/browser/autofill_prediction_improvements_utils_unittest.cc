// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_utils.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {

namespace {

// Test that an empty form is not eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleByFieldCriteria_EmptyForm) {
  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::FormStructureTestApi form_test_api(form);

  EXPECT_FALSE(IsFormEligibleByFieldCriteria(form));
}

// Test that a form with a single UNKNOWN_TYPE field is not eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleByFieldCriteria_SingleUnknownField) {
  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::FormStructureTestApi form_test_api(form);

  autofill::AutofillField& unknown_field = form_test_api.PushField();
  unknown_field.set_heuristic_type(autofill::GetActiveHeuristicSource(),
                                   autofill::UNKNOWN_TYPE);
}

// Test that a form with a single address field is not eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleByFieldCriteria_SingleAddressField) {
  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::FormStructureTestApi form_test_api(form);

  // Add an additional address field and verify that is also does not yield and
  // overall eligible form.
  autofill::AutofillField& address_field = form_test_api.PushField();
  address_field.set_heuristic_type(autofill::GetActiveHeuristicSource(),
                                   autofill::NAME_FIRST);
  EXPECT_FALSE(IsFormEligibleByFieldCriteria(form));
}

// Test that a form with an eligible field is overall eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleByFieldCriteria_SingleEligibleField) {
  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::FormStructureTestApi form_test_api(form);

  autofill::AutofillField& prediction_improvement_field =
      form_test_api.PushField();
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  prediction_improvement_field.set_heuristic_type(
      autofill::HeuristicSource::kPredictionImprovementRegexes,
      autofill::IMPROVED_PREDICTION);
#else
  prediction_improvement_field.set_heuristic_type(
      autofill::GetActiveHeuristicSource(), autofill::IMPROVED_PREDICTION);
#endif
  EXPECT_TRUE(IsFormEligibleByFieldCriteria(form));
}

// Test that a form with an eligible field is overall eligible.
TEST(AutofillPredictionImprovementsUtilsTest,
     IsFormEligibleByFieldCriteria_MixedFormWithEligibleField) {
  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::FormStructureTestApi form_test_api(form);

  autofill::AutofillField& unknown_field = form_test_api.PushField();
  unknown_field.set_heuristic_type(autofill::GetActiveHeuristicSource(),
                                   autofill::UNKNOWN_TYPE);

  autofill::AutofillField& address_field = form_test_api.PushField();
  address_field.set_heuristic_type(autofill::GetActiveHeuristicSource(),
                                   autofill::NAME_FIRST);

  autofill::AutofillField& prediction_improvement_field =
      form_test_api.PushField();
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  prediction_improvement_field.set_heuristic_type(
      autofill::HeuristicSource::kPredictionImprovementRegexes,
      autofill::IMPROVED_PREDICTION);
#else
  prediction_improvement_field.set_heuristic_type(
      autofill::GetActiveHeuristicSource(), autofill::IMPROVED_PREDICTION);
#endif
  EXPECT_TRUE(IsFormEligibleByFieldCriteria(form));
}

}  // namespace

}  // namespace autofill_prediction_improvements
