// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class AutofillFieldFillingStatsAndScoreMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

// Test the logging of the field-wise filling stats and the form-wise filling
// score for the different form types.
TEST_F(AutofillFieldFillingStatsAndScoreMetricsTest, FillingStatsAndScores) {
  FormData form = GetAndAddSeenForm(
      {.description_for_logging = "FieldFillingStats",
       .fields =
           {
               {.role = NAME_FULL,
                .value = u"First Middle Last",
                .is_autofilled = true},
               // Those two fields are going to be changed to a value of the
               // same type.
               {.role = NAME_FIRST, .value = u"First", .is_autofilled = true},
               {.role = NAME_LAST, .value = u"Last", .is_autofilled = true},
               // This field is going to be changed to a value of a different
               // type.
               {.role = NAME_FIRST, .value = u"First", .is_autofilled = true},
               // This field is going to be changed to another value of unknown
               // type.
               {.role = NAME_FIRST, .value = u"First", .is_autofilled = true},
               // This field is going to be changed to the empty value.
               {.role = NAME_MIDDLE, .value = u"Middle", .is_autofilled = true},
               // This field remains.
               {.role = NAME_LAST, .value = u"Last", .is_autofilled = true},
               // This following two fields are manually filled to a value of
               // type NAME_FIRST.
               {.role = NAME_FIRST, .value = u"Elvis", .is_autofilled = false},
               {.role = NAME_FIRST, .value = u"Elvis", .is_autofilled = false},
               // This one is manually filled to a value of type NAME_LAST.
               {.role = NAME_FIRST,
                .value = u"Presley",
                .is_autofilled = false},
               // This next three are manually filled to a value of
               // UNKNOWN_TYPE.
               {.role = NAME_FIRST,
                .value = u"Random Value",
                .is_autofilled = false},
               {.role = NAME_MIDDLE,
                .value = u"Random Value",
                .is_autofilled = false},
               {.role = NAME_LAST,
                .value = u"Random Value",
                .is_autofilled = false},
               // The last field is not autofilled and empty.
               {.role = ADDRESS_HOME_CITY,
                .value = u"",
                .is_autofilled = false},
               // We add two credit cards field to make sure those are counted
               // in separate statistics.
               {.role = CREDIT_CARD_NAME_FULL,
                .value = u"Test Name",
                .is_autofilled = true},
               {.role = CREDIT_CARD_NUMBER,
                .value = u"",
                .is_autofilled = false},
           },
       .unique_renderer_id = test::MakeFormRendererId(),
       .main_frame_origin =
           url::Origin::Create(autofill_client_->form_origin())});

  // Elvis is of type NAME_FIRST in the test profile.
  SimulateUserChangedTextFieldTo(form, form.fields[1], u"Elvis");
  // Presley is of type NAME_LAST in the test profile
  SimulateUserChangedTextFieldTo(form, form.fields[2], u"Presley");
  // Presley is of type NAME_LAST in the test profile
  SimulateUserChangedTextFieldTo(form, form.fields[3], u"Presley");
  // This is a random string of UNKNOWN_TYPE.
  SimulateUserChangedTextFieldTo(form, form.fields[4], u"something random");
  SimulateUserChangedTextFieldTo(form, form.fields[5], u"");

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  const std::string histogram_prefix = "Autofill.FieldFillingStats.Address.";

  // Testing of the FormFillingStats expectations.

  histogram_tester.ExpectUniqueSample(histogram_prefix + "Accepted", 2, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "CorrectedToSameType",
                                      2, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "CorrectedToDifferentType", 1, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "CorrectedToUnknownType", 1, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "CorrectedToEmpty", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "LeftEmpty", 1, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "ManuallyFilledToSameType", 2, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "ManuallyFilledToDifferentType", 1, 1);
  histogram_tester.ExpectUniqueSample(
      histogram_prefix + "ManuallyFilledToUnknownType", 3, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalManuallyFilled",
                                      6, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalFilled", 7, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalCorrected", 5,
                                      1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "TotalUnfilled", 7, 1);
  histogram_tester.ExpectUniqueSample(histogram_prefix + "Total", 14, 1);

  // Testing of the FormFillingScore expectations.

  // The form contains a total of 7 autofilled address fields. Two fields are
  // accepted while 5 are corrected.
  const int accepted_address_fields = 2;
  const int corrected_address_fields = 5;

  const int expected_address_score =
      2 * accepted_address_fields - 3 * corrected_address_fields + 100;
  const int expected_address_complex_score =
      accepted_address_fields * 10 + corrected_address_fields;

  histogram_tester.ExpectUniqueSample("Autofill.FormFillingScore.Address",
                                      expected_address_score, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormFillingComplexScore.Address",
      expected_address_complex_score, 1);

  // Also test for credit cards where there is exactly one accepted field and
  // no corrected fields.
  histogram_tester.ExpectUniqueSample("Autofill.FormFillingScore.CreditCard",
                                      102, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormFillingComplexScore.CreditCard", 10, 1);
}

}  // namespace autofill::autofill_metrics
