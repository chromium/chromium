// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/loyalty_card_form_event_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

using test::CreateTestFormField;

// Parameterized test where the parameter indicates how far we went through
// the funnel:
// 0 = Site contained form but user did not focus it (did not interact).
// 1 = User interacted with form (focused a field).
// 2 = User saw a suggestion to fill the form.
// 3 = User accepted the suggestion.
// 4 = User submitted the form.
class LoyaltyCardFormEventLoggerFunnelTest
    : public AutofillMetricsBaseTest,
      public testing::TestWithParam<int> {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         LoyaltyCardFormEventLoggerFunnelTest,
                         testing::Values(0, 1, 2, 3, 4));

TEST_P(LoyaltyCardFormEventLoggerFunnelTest, LogFunnelMetrics) {
  base::HistogramTester histogram_tester;
  // Set initial loyalty card data.
  const LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  test_api(valuables_data_manager()).SetLoyaltyCards({loyalty_card});
  const FormData form =
      CreateForm({CreateTestFormField("Loyalty Program", "loyalty-program", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Loyalty Number", "loyalty-number", "",
                                      FormControlType::kInputText)});

  // Phase 1: Simulate events according to GetParam().
  const bool user_interacted_with_form = GetParam() >= 1;
  const bool user_saw_suggestion = GetParam() >= 2;
  const bool user_accepted_suggestion = GetParam() >= 3;
  const bool user_submitted_form = GetParam() >= 4;

  // Simulate that the autofill manager has seen this form on page load.
  SeeForm(form);

  if (!user_saw_suggestion) {
    // Remove the profile to prevent suggestion from being shown.
    test_api(valuables_data_manager()).ClearLoyaltyCards();
  }

  // Simulate interacting with the form.
  if (user_interacted_with_form) {
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[1].global_id());
  }

  // Simulate seeing a suggestion.
  if (user_saw_suggestion) {
    DidShowAutofillSuggestions(form, /*field_index=*/1);
  }

  // Simulate filling the form.
  if (user_accepted_suggestion) {
    FillLoyaltyCard(form, loyalty_card, /*field_index=*/1);
  }

  if (user_submitted_form) {
    SubmitForm(form);
  }

  ResetDriverToCommitMetrics();

  // Phase 2: Validate Funnel expectations.
  histogram_tester.ExpectBucketCount("Autofill.Funnel.ParsedAsType.LoyaltyCard",
                                     1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Funnel.InteractionAfterParsedAsType.LoyaltyCard",
      user_interacted_with_form ? 1 : 0, 1);
  if (user_interacted_with_form) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.SuggestionAfterInteraction.LoyaltyCard",
        user_saw_suggestion ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.SuggestionAfterInteraction.LoyaltyCard", 0);
  }

  if (user_saw_suggestion) {
    // If the suggestion was shown, we should record whether the user
    // accepted it.
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.FillAfterSuggestion.LoyaltyCard",
        user_accepted_suggestion ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.FillAfterSuggestion.LoyaltyCard", 0);
  }

  if (user_accepted_suggestion) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.SubmissionAfterFill.LoyaltyCard",
        user_submitted_form ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.SubmissionAfterFill.LoyaltyCard", 0);
  }
}

TEST_P(LoyaltyCardFormEventLoggerFunnelTest, LogKeyMetrics) {
  base::HistogramTester histogram_tester;
  // Set initial loyalty card data.
  const LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  test_api(valuables_data_manager()).SetLoyaltyCards({loyalty_card});
  const FormData form =
      CreateForm({CreateTestFormField("Loyalty Program", "loyalty-program", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Loyalty Number", "loyalty-number", "",
                                      FormControlType::kInputText)});

  // Phase 1: Simulate events according to GetParam().
  const bool user_interacted_with_form = GetParam() >= 1;
  const bool user_saw_suggestion = GetParam() >= 2;
  const bool user_accepted_suggestion = GetParam() >= 3;
  const bool user_submitted_form = GetParam() >= 4;

  // Simulate that the autofill manager has seen this form on page load.
  SeeForm(form);

  if (!user_saw_suggestion) {
    // Remove the profile to prevent suggestion from being shown.
    test_api(valuables_data_manager()).ClearLoyaltyCards();
  }

  // Simulate interacting with the form.
  if (user_interacted_with_form) {
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[1].global_id());
  }

  // Simulate seeing a suggestion.
  if (user_saw_suggestion) {
    DidShowAutofillSuggestions(form, /*field_index=*/1);
  }

  // Simulate filling the form.
  if (user_accepted_suggestion) {
    FillLoyaltyCard(form, loyalty_card, /*field_index=*/1);
  }

  if (user_submitted_form) {
    SubmitForm(form);
  }

  ResetDriverToCommitMetrics();

  // Phase 2: Validate KeyMetrics expectations.
  if (user_submitted_form) {
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingReadiness.LoyaltyCard", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingAcceptance.LoyaltyCard", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingCorrectness.LoyaltyCard", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingAssistance.LoyaltyCard", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Autocomplete.NotOff.FillingAcceptance.LoyaltyCard", 1, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.Off.FillingAcceptance.LoyaltyCard", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType",
        GetBucketForAcceptanceMetricsGroupedByFieldType(
            LOYALTY_MEMBERSHIP_ID, /*suggestion_accepted=*/true),
        1);

  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingReadiness.LoyaltyCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAcceptance.LoyaltyCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingCorrectness.LoyaltyCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAssistance.LoyaltyCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.NotOff.FillingAcceptance.LoyaltyCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.Off.FillingAcceptance.LoyaltyCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType", 0);
  }
  if (user_accepted_suggestion) {
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FormSubmission.Autofilled.LoyaltyCard",
        user_submitted_form ? 1 : 0, 1);
  }
}

// Tests for Autofill.KeyMetrics.* metrics.
class LoyaltyCardFormEventLoggerBaseKeyMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override {
    SetUpHelper();

    test_api(valuables_data_manager())
        .SetLoyaltyCards(
            {test::CreateLoyaltyCard(), test::CreateLoyaltyCard2()});

    // Load a fillable form.
    form_ =
        CreateForm({CreateTestFormField("Loyalty Program", "loyalty-program",
                                        "", FormControlType::kInputText),
                    CreateTestFormField("Loyalty Number", "loyalty-number", "",
                                        FormControlType::kInputText)});

    field_types_ = {LOYALTY_MEMBERSHIP_PROGRAM, LOYALTY_MEMBERSHIP_ID};
    autofill_manager().AddSeenForm(form_, field_types_, field_types_);
  }

  void TearDown() override { TearDownHelper(); }

  // Fillable form.
  FormData form_;
  std::vector<FieldType> field_types_;
};

// Validate Autofill.KeyMetrics.* in case the user submits an empty form.
// Empty in the sense that the user did not fill/type into the fields (not that
// it has no fields).
TEST_F(LoyaltyCardFormEventLoggerBaseKeyMetricsTest, LogEmptyForm) {
  base::HistogramTester histogram_tester;

  // Simulate page load.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_,
                                              form_.fields()[0].global_id());
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.LoyaltyCard", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.LoyaltyCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.LoyaltyCard", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.LoyaltyCard", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.LoyaltyCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType", 0);
}

// Validate Autofill.KeyMetrics.* in case the user does not accept a suggestion.
TEST_F(LoyaltyCardFormEventLoggerBaseKeyMetricsTest,
       LogUserDoesNotAcceptSuggestion) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown but user does not accept it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_,
                                              form_.fields()[1].global_id());
  DidShowAutofillSuggestions(form_, /*field_index=*/1);

  SimulateUserChangedField(form_, form_.fields()[0]);
  SimulateUserChangedField(form_, form_.fields()[1]);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAcceptance.LoyaltyCard", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.LoyaltyCard", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.LoyaltyCard", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.LoyaltyCard", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType",
      GetBucketForAcceptanceMetricsGroupedByFieldType(
          field_types_[1], /*suggestion_accepted=*/false),
      1);
}
// Validate Autofill.KeyMetrics.* in case the user has to fix the filled data.
TEST_F(LoyaltyCardFormEventLoggerBaseKeyMetricsTest, LogUserFixesFilledData) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_,
                                              form_.fields()[1].global_id());
  DidShowAutofillSuggestions(form_, /*field_index=*/1);
  FillLoyaltyCard(form_, valuables_data_manager().GetLoyaltyCards()[0],
                  /*field_index=*/1);

  // Simulate user fixing the card.
  SimulateUserChangedField(form_, form_.fields()[1]);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAcceptance.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingCorrectness.LoyaltyCard", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.LoyaltyCard", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType",
      GetBucketForAcceptanceMetricsGroupedByFieldType(
          field_types_[1], /*suggestion_accepted=*/true),
      1);
}

// Validate Autofill.KeyMetrics.* in case the user fixes the filled data but
// then does not submit the form.
TEST_F(LoyaltyCardFormEventLoggerBaseKeyMetricsTest,
       LogUserFixesFilledDataButDoesNotSubmit) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_,
                                              form_.fields()[1].global_id());
  DidShowAutofillSuggestions(form_);
  FillLoyaltyCard(form_, valuables_data_manager().GetLoyaltyCards()[0],
                  /*field_index=*/1);

  // Simulate user fixing the card.
  SimulateUserChangedField(form_, form_.fields()[1]);

  // Don't submit form.
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingReadiness.LoyaltyCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.LoyaltyCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.LoyaltyCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAssistance.LoyaltyCard", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.LoyaltyCard", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType", 0);
}

}  // namespace autofill::autofill_metrics
