// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

using UkmAutofillKeyMetricsType = ukm::builders::Autofill_KeyMetrics;
using test::CreateTestFormField;

// Parameterized test where the parameter indicates how far we went through
// the funnel:
// 0 = Site contained form but user did not focus it (did not interact).
// 1 = User interacted with form (focused a field).
// 2 = User saw a suggestion to fill the form.
// 3 = User accepted the suggestion.
// 4 = User submitted the form.
class FormEventLoggerBaseFunnelTest : public AutofillMetricsBaseTest,
                                      public testing::TestWithParam<int> {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         FormEventLoggerBaseFunnelTest,
                         testing::Values(0, 1, 2, 3, 4));

TEST_P(FormEventLoggerBaseFunnelTest, LogFunnelMetrics) {
  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});
  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  base::HistogramTester histogram_tester;

  // Phase 1: Simulate events according to GetParam().
  const bool user_interacted_with_form = GetParam() >= 1;
  const bool user_saw_suggestion = GetParam() >= 2;
  const bool user_accepted_suggestion = GetParam() >= 3;
  const bool user_submitted_form = GetParam() >= 4;

  // Simulate that the autofill manager has seen this form on page load.
  SeeForm(form);

  if (!user_saw_suggestion) {
    // Remove the profile to prevent suggestion from being shown.
    personal_data().ClearProfiles();
  }

  // Simulate interacting with the form.
  if (user_interacted_with_form) {
    autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);
  }

  // Simulate seeing a suggestion.
  if (user_saw_suggestion) {
    DidShowAutofillSuggestions(form);
  }

  // Simulate filling the form.
  if (user_accepted_suggestion) {
    FillTestProfile(form);
  }

  if (user_submitted_form) {
    SubmitForm(form);
  }

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).address_form_interactions_flow_id();
  ResetDriverToCommitMetrics();

  // Phase 2: Validate Funnel expectations.
  histogram_tester.ExpectBucketCount("Autofill.Funnel.ParsedAsType.Address", 1,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.Funnel.ParsedAsType.CreditCard",
                                     0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Funnel.InteractionAfterParsedAsType.Address",
      user_interacted_with_form ? 1 : 0, 1);
  if (user_interacted_with_form) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.SuggestionAfterInteraction.Address",
        user_saw_suggestion ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.SuggestionAfterInteraction.Address", 0);
  }

  if (user_saw_suggestion) {
    // If the suggestion was shown, we should record whether the user
    // accepted it.
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.FillAfterSuggestion.Address",
        user_accepted_suggestion ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.FillAfterSuggestion.Address", 0);
  }

  if (user_accepted_suggestion) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Funnel.SubmissionAfterFill.Address",
        user_submitted_form ? 1 : 0, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Funnel.SubmissionAfterFill.Address", 0);
  }

  // Phase 3: Validate KeyMetrics expectations.
  if (user_submitted_form) {
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingAcceptance.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingCorrectness.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FillingAssistance.Address", 1, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.Autocomplete.NotOff.FillingAcceptance.Address", 1, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.Off.FillingAcceptance.Address", 0);
    VerifyUkm(
        &test_ukm_recorder(), form, UkmAutofillKeyMetricsType::kEntryName,
        {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
          {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 1},
          {UkmAutofillKeyMetricsType::kFillingCorrectnessName, 1},
          {UkmAutofillKeyMetricsType::kFillingAssistanceName, 1},
          {UkmAutofillKeyMetricsType::kAutofillFillsName, 1},
          {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 0},
          {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
          {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingReadiness.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAssistance.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.NotOff.FillingAcceptance.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.Autocomplete.Off.FillingAcceptance.Address", 0);
  }
  if (user_accepted_suggestion) {
    histogram_tester.ExpectBucketCount(
        "Autofill.KeyMetrics.FormSubmission.Autofilled.Address",
        user_submitted_form ? 1 : 0, 1);
  }
}

// Verify that no key metrics are logged in the ablation state.
TEST_F(FormEventLoggerBaseFunnelTest, AblationState) {
  base::FieldTrialParams feature_parameters{
      {features::kAutofillAblationStudyEnabledForAddressesParam.name, "true"},
      {features::kAutofillAblationStudyEnabledForPaymentsParam.name, "true"},
      {features::kAutofillAblationStudyAblationWeightPerMilleParam.name,
       "1000"}};
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillEnableAblationStudy, feature_parameters);

  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});
  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  base::HistogramTester histogram_tester;

  // Simulate that the autofill manager has seen this form on page load.
  SeeForm(form);

  // Simulate interacting with the form.
  autofill_manager().OnAskForValuesToFillTest(form, form.fields[0]);

  // Don't simulate a suggestion but simulate the user typing.
  SimulateUserChangedTextField(form, form.fields[0]);

  SubmitForm(form);

  ResetDriverToCommitMetrics();

  // Phase 2: Validate Funnel expectations.
  const char* kMetrics[] = {"Autofill.Funnel.ParsedAsType",
                            "Autofill.Funnel.InteractionAfterParsedAsType",
                            "Autofill.Funnel.SuggestionAfterInteraction",
                            "Autofill.Funnel.FillAfterSuggestion",
                            "Autofill.Funnel.SubmissionAfterFill",
                            "Autofill.KeyMetrics.FillingReadiness",
                            "Autofill.KeyMetrics.FillingAcceptance",
                            "Autofill.KeyMetrics.FillingCorrectness",
                            "Autofill.KeyMetrics.FillingAssistance",
                            "Autofill.Autocomplete.NotOff.FillingAcceptance",
                            "Autofill.Autocomplete.Off.FillingAcceptance"};
  for (const char* metric : kMetrics) {
    histogram_tester.ExpectTotalCount(base::StrCat({metric, ".Address"}), 0);
    histogram_tester.ExpectTotalCount(base::StrCat({metric, ".CreditCard"}), 0);
  }
}

// Tests for Autofill.KeyMetrics.* metrics.
class FormEventLoggerBaseKeyMetricsTest : public AutofillMetricsBaseTest,
                                          public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override { TearDownHelper(); }

  // Fillable form.
  FormData form_;
};

void FormEventLoggerBaseKeyMetricsTest::SetUp() {
  SetUpHelper();

  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  // Load a fillable form.
  form_ = CreateEmptyForm();
  form_.fields = {
      CreateTestFormField("State", "state", "", FormControlType::kInputText),
      CreateTestFormField("City", "city", "", FormControlType::kInputText),
      CreateTestFormField("Street", "street", "", FormControlType::kInputText)};
  std::vector<ServerFieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form_, field_types, field_types);
}

// Validate Autofill.KeyMetrics.* in case the user submits the empty form.
// Empty in the sense that the user did not fill/type into the fields (not that
// it has no fields).
TEST_F(FormEventLoggerBaseKeyMetricsTest, LogEmptyForm) {
  base::HistogramTester histogram_tester;

  // Simulate page load.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);

  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).address_form_interactions_flow_id();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.Address", 0);

  VerifyUkm(&test_ukm_recorder(), form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 0},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user has no address profile on
// file, so nothing can be filled.
TEST_F(FormEventLoggerBaseKeyMetricsTest, LogNoProfile) {
  base::HistogramTester histogram_tester;

  // Simulate that no data is available.
  personal_data().ClearProfiles();
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);

  SimulateUserChangedTextField(form_, form_.fields[0]);
  SimulateUserChangedTextField(form_, form_.fields[1]);
  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).address_form_interactions_flow_id();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.Address", 1, 1);

  VerifyUkm(&test_ukm_recorder(), form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 2},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user does not accept a suggestion.
TEST_F(FormEventLoggerBaseKeyMetricsTest, LogUserDoesNotAcceptSuggestion) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown but user does not accept it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);

  SimulateUserChangedTextField(form_, form_.fields[0]);
  SimulateUserChangedTextField(form_, form_.fields[1]);
  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).address_form_interactions_flow_id();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.NotAutofilled.Address", 1, 1);

  VerifyUkm(&test_ukm_recorder(), form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
              {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 2},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user has to fix the filled data.
TEST_F(FormEventLoggerBaseKeyMetricsTest, LogUserFixesFilledData) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);

  // Simulate user fixing the address.
  SimulateUserChangedTextField(form_, form_.fields[1]);
  SubmitForm(form_);

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).address_form_interactions_flow_id();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.Address", 1, 1);

  VerifyUkm(&test_ukm_recorder(), form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 1},
              {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 1},
              {UkmAutofillKeyMetricsType::kFillingCorrectnessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 1},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 1},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 1},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

// Validate Autofill.KeyMetrics.* in case the user fixes the filled data but
// then does not submit the form.
TEST_F(FormEventLoggerBaseKeyMetricsTest,
       LogUserFixesFilledDataButDoesNotSubmit) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);

  // Simulate user fixing the address.
  SimulateUserChangedTextField(form_, form_.fields[1]);

  // Don't submit form.

  FormInteractionsFlowId flow_id =
      test_api(autofill_manager()).address_form_interactions_flow_id();
  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingReadiness.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.Address", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAssistance.Address", 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.KeyMetrics.FormSubmission.Autofilled.Address", 0, 1);

  VerifyUkm(&test_ukm_recorder(), form_, UkmAutofillKeyMetricsType::kEntryName,
            {{{UkmAutofillKeyMetricsType::kFillingReadinessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAcceptanceName, 0},
              {UkmAutofillKeyMetricsType::kFillingCorrectnessName, 0},
              {UkmAutofillKeyMetricsType::kFillingAssistanceName, 0},
              {UkmAutofillKeyMetricsType::kAutofillFillsName, 0},
              {UkmAutofillKeyMetricsType::kFormElementUserModificationsName, 0},
              {UkmAutofillKeyMetricsType::kFlowIdName, flow_id.value()},
              {UkmAutofillKeyMetricsType::kFormTypesName, 2}}});
}

TEST_F(FormEventLoggerBaseKeyMetricsTest, NoEmailOnlyLeakage) {
  base::HistogramTester histogram_tester;
  // Reset `form_` to be of the type that the email heuristic only metric is
  // interested in. With the feature off, that metric should not be logged.
  form_ = test::GetFormData({.fields = {{.role = EMAIL_ADDRESS}}});

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();
  histogram_tester.ExpectTotalCount("Autofill.EmailHeuristicOnlyAcceptance", 0);
}

// Tests for Autofill.EmailHeuristicOnlyAcceptance. That metric is only written
// when the form meets the email heuristic criteria and the feature is enabled.
class FormEventLoggerBaseEmailHeuristicOnlyMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override { TearDownHelper(); }

  // Fillable form.
  FormData form_;
  base::test::ScopedFeatureList features_{
      features::kAutofillEnableEmailHeuristicOnlyAddressForms};
};

void FormEventLoggerBaseEmailHeuristicOnlyMetricsTest::SetUp() {
  SetUpHelper();

  // Create a profile.
  RecreateProfile(/*is_server=*/false);

  // Load a fillable form.
  form_ = test::GetFormData({.fields = {{.role = EMAIL_ADDRESS}}});
  std::vector<ServerFieldType> heuristic_types = {EMAIL_ADDRESS};
  std::vector<ServerFieldType> server_types = {NO_SERVER_DATA};

  autofill_manager().AddSeenForm(form_, heuristic_types, server_types);
}

TEST_F(FormEventLoggerBaseEmailHeuristicOnlyMetricsTest, UserDoesNotAccept) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown but user does not accept it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount("Autofill.EmailHeuristicOnlyAcceptance", 0,
                                     1);
}

TEST_F(FormEventLoggerBaseEmailHeuristicOnlyMetricsTest, UserAccepts) {
  base::HistogramTester histogram_tester;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectBucketCount("Autofill.EmailHeuristicOnlyAcceptance", 1,
                                     1);
}

TEST_F(FormEventLoggerBaseEmailHeuristicOnlyMetricsTest, NoEmailField) {
  base::HistogramTester histogram_tester;

  // Reset the form to exclude any email addresses.
  form_ = test::GetFormData({.fields = {{.role = NAME_FULL}}});

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount("Autofill.EmailHeuristicOnlyAcceptance", 0);
}

TEST_F(FormEventLoggerBaseEmailHeuristicOnlyMetricsTest, ServerTypeKnown) {
  base::HistogramTester histogram_tester;

  // Reset the form to include only a known server type.
  form_ = test::GetFormData({.fields = {{.role = EMAIL_ADDRESS}}});
  std::vector<ServerFieldType> field_types = {EMAIL_ADDRESS};
  autofill_manager().AddSeenForm(form_, field_types, field_types);

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount("Autofill.EmailHeuristicOnlyAcceptance", 0);
}

TEST_F(FormEventLoggerBaseEmailHeuristicOnlyMetricsTest, NotFormTag) {
  base::HistogramTester histogram_tester;

  // Set the form to appear outside a <form> tag, which means it is not eligible
  // for the email heuristic only metric.
  form_.is_form_tag = false;

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount("Autofill.EmailHeuristicOnlyAcceptance", 0);
}

TEST_F(FormEventLoggerBaseEmailHeuristicOnlyMetricsTest, TooManyFields) {
  base::HistogramTester histogram_tester;

  // Reset the form to exceed the heuristic minimum, meaning it does not meet
  // the requirements to be counted in the `EmailHeuristicOnlyAcceptance`
  // metric.
  form_ = test::GetFormData({.fields = {{.role = NAME_FULL},
                                        {.role = EMAIL_ADDRESS},
                                        {.role = NAME_SUFFIX}}});

  // Simulate that suggestion is shown and user accepts it.
  SeeForm(form_);
  autofill_manager().OnAskForValuesToFillTest(form_, form_.fields[0]);
  DidShowAutofillSuggestions(form_);
  FillTestProfile(form_);
  SubmitForm(form_);

  ResetDriverToCommitMetrics();

  histogram_tester.ExpectTotalCount("Autofill.EmailHeuristicOnlyAcceptance", 0);
}

}  // namespace autofill::autofill_metrics
