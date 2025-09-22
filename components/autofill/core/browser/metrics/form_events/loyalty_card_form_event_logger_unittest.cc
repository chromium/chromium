// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/loyalty_card_form_event_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

using UkmAutofillKeyMetricsType = ukm::builders::Autofill_KeyMetrics;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using UkmInteractedWithFormType = ukm::builders::Autofill_InteractedWithForm;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using test::CreateTestFormField;
using ::testing::Each;
using ::testing::IsEmpty;

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

  DeleteDriverToCommitMetrics();

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

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).loyalty_card_form_interactions_flow_id();
  DeleteDriverToCommitMetrics();

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

    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({{{Ukm::kFillingReadinessName, 1},
                       {Ukm::kFillingAcceptanceName, 1},
                       {Ukm::kFillingCorrectnessName, 1},
                       {Ukm::kFillingAssistanceName, 1},
                       {Ukm::kAutofillFillsName, 1},
                       {Ukm::kFormElementUserModificationsName, 0},
                       {Ukm::kFlowIdName, flow_id.value()},
                       {Ukm::kFormTypesName,
                        AutofillMetrics::FormTypesToBitVector(
                            {FormTypeNameForLogging::kLoyaltyCardForm})}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
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

    EXPECT_THAT(GetUkmEvents(test_ukm_recorder(),
                             UkmAutofillKeyMetricsType::kEntryName),
                UkmEventsAre({}));
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

  void VerifyInteractedWithFormUkmMetric() {
    using Ukm = UkmInteractedWithFormType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({{{Ukm::kIsForCreditCardName, false},
                       {Ukm::kLocalRecordTypeCountName, 2},
                       {Ukm::kServerRecordTypeCountName, 0},
                       {Ukm::kFormSignatureName,
                        Collapse(CalculateFormSignature(form_)).value()}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

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

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).loyalty_card_form_interactions_flow_id();
  DeleteDriverToCommitMetrics();

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

  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({{{Ukm::kFillingReadinessName, 1},
                       {Ukm::kFillingAssistanceName, 0},
                       {Ukm::kAutofillFillsName, 0},
                       {Ukm::kFormElementUserModificationsName, 0},
                       {Ukm::kFlowIdName, flow_id.value()},
                       {Ukm::kFormTypesName,
                        AutofillMetrics::FormTypesToBitVector(
                            {FormTypeNameForLogging::kLoyaltyCardForm})}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  EXPECT_THAT(
      GetUkmEvents(test_ukm_recorder(), UkmSuggestionFilledType::kEntryName),
      UkmEventsAre({}));

  VerifyInteractedWithFormUkmMetric();
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

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).loyalty_card_form_interactions_flow_id();
  DeleteDriverToCommitMetrics();

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

  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({{{Ukm::kFillingReadinessName, 1},
                       {Ukm::kFillingAcceptanceName, 0},
                       {Ukm::kFillingAssistanceName, 0},
                       {Ukm::kAutofillFillsName, 0},
                       {Ukm::kFormElementUserModificationsName, 2},
                       {Ukm::kFlowIdName, flow_id.value()},
                       {Ukm::kFormTypesName,
                        AutofillMetrics::FormTypesToBitVector(
                            {FormTypeNameForLogging::kLoyaltyCardForm})}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  EXPECT_THAT(
      GetUkmEvents(test_ukm_recorder(), UkmSuggestionFilledType::kEntryName),
      UkmEventsAre({}));

  VerifyInteractedWithFormUkmMetric();
}

// Validate Autofill.KeyMetrics.* in case the user has filled a suggestion.
TEST_F(LoyaltyCardFormEventLoggerBaseKeyMetricsTest, UserAcceptsSuggestion) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_,
                                              form_.fields()[1].global_id());
  DidShowAutofillSuggestions(form_, /*field_index=*/1);
  FillLoyaltyCard(form_, valuables_data_manager().GetLoyaltyCards()[0],
                  /*field_index=*/1);

  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).loyalty_card_form_interactions_flow_id();
  DeleteDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAcceptance.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingCorrectness.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.LoyaltyCard", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.LoyaltyCard", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.KeyMetrics.FillingAcceptance.GroupedByFocusedFieldType",
      GetBucketForAcceptanceMetricsGroupedByFieldType(
          field_types_[1], /*suggestion_accepted=*/true),
      1);

  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({{{Ukm::kFillingReadinessName, 1},
                       {Ukm::kFillingAcceptanceName, 1},
                       {Ukm::kFillingCorrectnessName, 1},
                       {Ukm::kFillingAssistanceName, 1},
                       {Ukm::kAutofillFillsName, 1},
                       {Ukm::kFormElementUserModificationsName, 0},
                       {Ukm::kFlowIdName, flow_id.value()},
                       {Ukm::kFormTypesName,
                        AutofillMetrics::FormTypesToBitVector(
                            {FormTypeNameForLogging::kLoyaltyCardForm})}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  {
    using Ukm = UkmSuggestionFilledType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kIsForCreditCardName, false},
              {Ukm::kFormSignatureName,
               Collapse(CalculateFormSignature(form_)).value()},
              {Ukm::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form_.fields()[1]))
                   .value()},
              {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  // Verify that the FORM_EVENT_LOCAL_SUGGESTION_FILLED and
  // FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE events are logged by the logger,
  // other events are logged by the base logger.
  {
    using Ukm = UkmFormEventType;
    auto event_metrics = [](FormEvent e) {
      return std::vector<UkmMetricNameAndValue>{
          {Ukm::kAutofillFormEventName, e},
          {Ukm::kFormTypesName,
           AutofillMetrics::FormTypesToBitVector(
               {FormTypeNameForLogging::kLoyaltyCardForm})},
          {Ukm::kMillisecondsSinceFormParsedName, 0}};
    };
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {event_metrics(FORM_EVENT_DID_PARSE_FORM),
             event_metrics(FORM_EVENT_DID_PARSE_FORM),
             event_metrics(FORM_EVENT_INTERACTED_ONCE),
             event_metrics(FORM_EVENT_SUGGESTIONS_SHOWN),
             event_metrics(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE),
             event_metrics(FORM_EVENT_LOCAL_SUGGESTION_FILLED),
             event_metrics(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE),
             event_metrics(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE),
             event_metrics(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE),
             event_metrics(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE),
             event_metrics(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE)}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  VerifyInteractedWithFormUkmMetric();
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

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).loyalty_card_form_interactions_flow_id();
  DeleteDriverToCommitMetrics();

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

  {
    using Ukm = UkmAutofillKeyMetricsType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({{{Ukm::kFillingReadinessName, 1},
                       {Ukm::kFillingAcceptanceName, 1},
                       {Ukm::kFillingCorrectnessName, 0},
                       {Ukm::kFillingAssistanceName, 1},
                       {Ukm::kAutofillFillsName, 1},
                       {Ukm::kFormElementUserModificationsName, 1},
                       {Ukm::kFlowIdName, flow_id.value()},
                       {Ukm::kFormTypesName,
                        AutofillMetrics::FormTypesToBitVector(
                            {FormTypeNameForLogging::kLoyaltyCardForm})}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  {
    using Ukm = UkmSuggestionFilledType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kIsForCreditCardName, false},
              {Ukm::kFormSignatureName,
               Collapse(CalculateFormSignature(form_)).value()},
              {Ukm::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form_.fields()[1]))
                   .value()},
              {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  VerifyInteractedWithFormUkmMetric();
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
  DeleteDriverToCommitMetrics();

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

  EXPECT_THAT(
      GetUkmEvents(test_ukm_recorder(), UkmAutofillKeyMetricsType::kEntryName),
      UkmEventsAre({}));

  {
    using Ukm = UkmSuggestionFilledType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kIsForCreditCardName, false},
              {Ukm::kFormSignatureName,
               Collapse(CalculateFormSignature(form_)).value()},
              {Ukm::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form_.fields()[1]))
                   .value()},
              {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form_.main_frame_origin().GetURL()));
  }

  VerifyInteractedWithFormUkmMetric();
}

// Parameterized AffiliationTypeKeyMetricsEditTest that edits a field depending
// on the parameter. This is used to test the correctness metric, which depends
// on whether autofilled fields have been edited. Additionally, these tests
// verify that the category-resolved assistance, acceptance and readiness
// metrics are correctly emitted.
class AffiliationTypeKeyMetricsEditTest
    : public LoyaltyCardFormEventLoggerBaseKeyMetricsTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldEditField() const { return GetParam(); }

  void FillAndSubmitForm(int selected_suggestion) {
    SeeForm(form_);
    autofill_manager().OnAskForValuesToFillTest(form_,
                                                form_.fields()[1].global_id());
    DidShowAutofillSuggestions(form_, /*field_index=*/1);
    FillLoyaltyCard(
        form_, valuables_data_manager().GetLoyaltyCards()[selected_suggestion],
        /*field_index=*/1);

    if (ShouldEditField()) {
      SimulateUserChangedField(form_, form_.fields()[1]);
    }
    SubmitForm(form_);
  }

 protected:
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(, AffiliationTypeKeyMetricsEditTest, testing::Bool());

// Tests the scenario where only affiliated cards are offered to the user.
// Validates affiliation key metrics when an affiliated card is available and
// selected.
TEST_P(AffiliationTypeKeyMetricsEditTest, Affiliated) {
  // Make sure that the card is an affiliated card.
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://affiliated.com"));
  const LoyaltyCard card1 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
      /*merchant_name=*/"CVS Pharmacy",
      /*program_name=*/"CVS Extra",
      /*program_logo=*/GURL(""),
      /*loyalty_card_number=*/"987654321987654321",
      {GURL("https://affiliated.com")});
  test_api(valuables_data_manager()).SetLoyaltyCards({card1});

  FillAndSubmitForm(/*selected_suggestion=*/0);

  DeleteDriverToCommitMetrics();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingReadinessAffiliationCategory",
      AffiliationCategoryMetricBucket::kAffiliated, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingAssistance.Affiliated", 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingCorrectness.Affiliated", !ShouldEditField(),
      1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.NonAffiliated", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.Mixed", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingAcceptance.Affiliated", 1);
}

// Tests the scenario where only non-affiliated cards are offered to the user.
// Validates affiliation key metrics when non-affiliated card is available and
// selected.
TEST_P(AffiliationTypeKeyMetricsEditTest, NonAffiliated) {
  // Make sure that the card is a non-affiliated card.
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://non-affiliated.com"));
  const LoyaltyCard card1 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
      /*merchant_name=*/"CVS Pharmacy",
      /*program_name=*/"CVS Extra",
      /*program_logo=*/GURL(""),
      /*loyalty_card_number=*/"987654321987654321",
      {GURL("https://affiliated.com")});
  test_api(valuables_data_manager()).SetLoyaltyCards({card1});

  FillAndSubmitForm(/*selected_suggestion=*/0);

  DeleteDriverToCommitMetrics();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingReadinessAffiliationCategory",
      AffiliationCategoryMetricBucket::kNonAffiliated, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingAssistance.Affiliated", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.Affiliated", 0);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingCorrectness.NonAffiliated",
      !ShouldEditField(), 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.Mixed", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingAcceptance.Affiliated", 0);
}

// Tests the scenario where both affiliated and non-affiliated cards are offered
// to the user. Validates affiliation key metrics when the affiliated card is
// selected by the user.
TEST_P(AffiliationTypeKeyMetricsEditTest, MixedAvailabilityAffiliatedSelected) {
  // Make sure that at least one card is an affiliated card.
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://affiliated.com"));
  const LoyaltyCard card1 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("1"),
      /*merchant_name=*/"CVS Pharmacy",
      /*program_name=*/"CVS Extra",
      /*program_logo=*/GURL(""),
      /*loyalty_card_number=*/"98765432198", {GURL("https://affiliated.com")});
  const LoyaltyCard card2 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("2"),
      /*merchant_name=*/"Walgreens",
      /*program_name=*/"CustomerCard",
      /*program_logo=*/GURL(""),
      /*loyalty_card_number=*/"998766823", {GURL("https://example.com")});
  test_api(valuables_data_manager()).SetLoyaltyCards({card1, card2});

  // Selects the affiliated card.
  FillAndSubmitForm(/*selected_suggestion=*/0);

  DeleteDriverToCommitMetrics();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingReadinessAffiliationCategory",
      AffiliationCategoryMetricBucket::kMixed, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingAssistance.Affiliated", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingCorrectness.Affiliated", !ShouldEditField(),
      1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.NonAffiliated", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.Mixed", 0);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingAcceptance.Affiliated", true, 1);
}

// Tests the scenario where both affiliated and non-affiliated cards are offered
// to the user. Validates affiliation key metrics when the non-affiliated card
// is selected by the user.
TEST_P(AffiliationTypeKeyMetricsEditTest,
       MixedAvailabilityNonAffiliatedSelected) {
  base::HistogramTester histogram_tester;
  // Make sure that at least one card is an affiliated card.
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://affiliated.com"));
  const LoyaltyCard card1 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("1"),
      /*merchant_name=*/"CVS Pharmacy",
      /*program_name=*/"CVS Extra",
      /*program_logo=*/GURL(""),
      /*loyalty_card_number=*/"987654321987654321",
      {GURL("https://affiliated.com")});
  const LoyaltyCard card2 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("2"),
      /*merchant_name=*/"Walgreens",
      /*program_name=*/"CustomerCard",
      /*program_logo=*/GURL(""),
      /*loyalty_card_number=*/"998766823", {GURL("https://example.com")});
  test_api(valuables_data_manager()).SetLoyaltyCards({card1, card2});

  // Selects the non-affiliated card.
  FillAndSubmitForm(/*selected_suggestion=*/1);

  DeleteDriverToCommitMetrics();

  histogram_tester.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingReadinessAffiliationCategory",
      AffiliationCategoryMetricBucket::kMixed, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingAssistance.Affiliated", false, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.Affiliated", 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingCorrectness.NonAffiliated",
      !ShouldEditField(), 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.LoyaltyCard.FillingCorrectness.Mixed", 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LoyaltyCard.FillingAcceptance.Affiliated", false, 1);
}

}  // namespace autofill::autofill_metrics
