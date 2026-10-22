// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "components/android_autofill/browser/android_form_event_logger.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AndroidFormEventLoggerTest : public testing::Test {
 public:
  ~AndroidFormEventLoggerTest() override = default;
};

class AndroidFormEventLoggerFunnelTest : public testing::TestWithParam<int> {
 public:
  ~AndroidFormEventLoggerFunnelTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(AndroidFormEventLoggerTest,
                         AndroidFormEventLoggerFunnelTest,
                         testing::Values(0, 1, 2, 3, 4));

TEST_P(AndroidFormEventLoggerFunnelTest, LogFunnelAndKeyMetrics) {
  // Phase 1: Simulate events according to GetParam().
  const bool parsed_form = GetParam() >= 1;
  const bool user_interacted_with_form = GetParam() >= 2;
  const bool user_accepted_suggestion = GetParam() >= 3;
  const bool user_submitted_form = GetParam() >= 4;

  base::HistogramTester histogram_tester;

  {
    AndroidFormEventLogger logger("FormType");
    if (parsed_form) {
      logger.OnDidParseForm();
    }

    if (user_interacted_with_form) {
      logger.OnDidInteractWithAutofillableForm();
    }

    if (user_accepted_suggestion) {
      logger.OnDidFillSuggestion();
    }

    if (user_submitted_form) {
      logger.OnWillSubmitForm();
    }

    // `logger` records metrics in destructor.
  }

  // Phase 2: Check which metrics where recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.WebView.Funnel.ParsedAsType2.FormType"),
              BucketsAre(base::Bucket(parsed_form, 1)));

  if (parsed_form) {
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.WebView.Funnel.InteractionAfterParsedAsType2.FormType"),
        BucketsAre(base::Bucket(user_interacted_with_form, 1)));
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.WebView.Funnel.InteractionAfterParsedAsType2.FormType", 0u);
  }

  if (user_interacted_with_form) {
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Autofill.WebView.Funnel.FillAfterInteraction2.FormType"),
                BucketsAre(base::Bucket(user_accepted_suggestion, 1)));
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.WebView.Funnel.FillAfterInteraction2.FormType", 0u);
  }

  if (user_submitted_form) {
    // `user_submitted_form` == true && `user_accepted_suggestion` == false
    // is tested in a different test.
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Autofill.WebView.KeyMetrics.FillingAssistance2.FormType"),
                BucketsAre(base::Bucket(user_accepted_suggestion, 1)));

    // A different test tests the user editing an autofilled field.
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Autofill.WebView.KeyMetrics.FillingCorrectness2.FormType"),
                BucketsAre(base::Bucket(true, 1)));

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.WebView.KeyMetrics.FormSubmission.Autofilled2.FormType"),
        BucketsAre(base::Bucket(true, 1)));
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.WebView.KeyMetrics.FillingAssistance2.FormType", 0u);
    histogram_tester.ExpectTotalCount(
        "Autofill.WebView.KeyMetrics.FillingCorrectness2.FormType", 0u);

    if (user_accepted_suggestion) {
      EXPECT_THAT(
          histogram_tester.GetAllSamples("Autofill.WebView.KeyMetrics."
                                         "FormSubmission.Autofilled2.FormType"),
          BucketsAre(base::Bucket(false, 1)));
    } else {
      histogram_tester.ExpectTotalCount(
          "Autofill.WebView.KeyMetrics.FormSubmission.Autofilled2.FormType",
          0u);
    }
  }
}

// Test that Autofill.WebView.KeyMetrics.FillingCorrectness is correctly
// recorded in the scenario that the user edits an autofilled field.
TEST_F(AndroidFormEventLoggerTest, FillingCorrectnessEditedAutofilledField) {
  base::HistogramTester histogram_tester;

  {
    AndroidFormEventLogger logger("FormType");
    logger.OnDidParseForm();
    logger.OnDidInteractWithAutofillableForm();
    logger.OnDidFillSuggestion();
    logger.OnEditedAutofilledField();
    logger.OnWillSubmitForm();

    // `logger` records metrics in destructor.
  }

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.WebView.KeyMetrics.FillingCorrectness2.FormType"),
              BucketsAre(base::Bucket(false, 1)));
}

// Test that Autofill.WebView.FillingAssistance metric is correctly recorded in
// the scenario that the user interacts with the form and submits it but does
// not use autofill.
TEST_F(AndroidFormEventLoggerTest,
       FillingAssistanceFormSubmissionInteractedNoAutofill) {
  base::HistogramTester histogram_tester;

  {
    AndroidFormEventLogger logger("FormType");
    logger.OnDidParseForm();
    logger.OnDidInteractWithAutofillableForm();
    logger.OnWillSubmitForm();

    // `logger` records metrics in destructor.
  }

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.WebView.KeyMetrics.FillingAssistance2.FormType"),
              BucketsAre(base::Bucket(false, 1)));
  histogram_tester.ExpectTotalCount(
      "Autofill.WebView.KeyMetrics.FillingAcceptance.FormType", 0u);
}

// Test that Autofill.WebView.KeyMetrics.FillingAcceptance is recorded when
// suggestions are shown and accepted before form submission.
TEST_F(AndroidFormEventLoggerTest, FillingAcceptanceSuggestionShownAndFilled) {
  base::HistogramTester histogram_tester;

  {
    AndroidFormEventLogger logger("FormType");
    logger.OnDidParseForm();
    logger.OnDidInteractWithAutofillableForm();
    logger.OnDidShowSuggestions();
    logger.OnDidFillSuggestion();
    logger.OnWillSubmitForm();
  }

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.WebView.KeyMetrics.FillingAcceptance.FormType"),
              BucketsAre(base::Bucket(true, 1)));
}

// Test that Autofill.WebView.KeyMetrics.FillingAcceptance is recorded as false
// when suggestions are shown and ignored before form submission.
TEST_F(AndroidFormEventLoggerTest,
       FillingAcceptanceSuggestionShownAndNotFilled) {
  base::HistogramTester histogram_tester;

  {
    AndroidFormEventLogger logger("FormType");
    logger.OnDidParseForm();
    logger.OnDidInteractWithAutofillableForm();
    logger.OnDidShowSuggestions();
    logger.OnWillSubmitForm();
  }

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.WebView.KeyMetrics.FillingAcceptance.FormType"),
              BucketsAre(base::Bucket(false, 1)));
}

// Test that Autofill.WebView.KeyMetrics.FormSubmission metric is correctly
// recorded in the scenario that the user manually fills the form (without the
// help of autofill) and submits it.
TEST_F(AndroidFormEventLoggerTest, FormSubmissionManualFill) {
  base::HistogramTester histogram_tester;

  {
    AndroidFormEventLogger logger("FormType");
    logger.OnDidParseForm();
    logger.OnDidInteractWithAutofillableForm();
    logger.OnTypedIntoNonFilledField();
    logger.OnWillSubmitForm();

    // `logger` records metrics in destructor.
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.WebView.KeyMetrics.FormSubmission.NotAutofilled2.FormType"),
      BucketsAre(base::Bucket(true, 1)));
}

// Test that Autofill.WebView.KeyMetrics.FormSubmission metric is correctly
// recorded in the scenario that the user manually fills the form and navigates
// away from the page without submitting the form.
TEST_F(AndroidFormEventLoggerTest, FilledFormNavigatedAway) {
  base::HistogramTester histogram_tester;

  {
    AndroidFormEventLogger logger("FormType");
    logger.OnDidParseForm();
    logger.OnDidInteractWithAutofillableForm();
    logger.OnTypedIntoNonFilledField();

    // `logger` records metrics in destructor.
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.WebView.KeyMetrics.FormSubmission.NotAutofilled2.FormType"),
      BucketsAre(base::Bucket(false, 1)));
}

}  // namespace autofill
