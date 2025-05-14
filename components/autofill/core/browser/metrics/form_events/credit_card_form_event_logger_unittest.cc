// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

using ::autofill::test::CreateTestFormField;
using ::base::Bucket;
using ::base::BucketsInclude;

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
using UkmSuggestionsShownType = ukm::builders::Autofill_SuggestionsShown;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmTextFieldValueChangedType = ukm::builders::Autofill_TextFieldDidChange;

class CreditCardFormEventLoggerTest : public AutofillMetricsBaseTest,
                                      public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

  CreditCard GetVirtualCreditCard(const std::string& guid) {
    CreditCard copy =
        *personal_data().payments_data_manager().GetCreditCardByGUID(guid);
    copy.set_record_type(CreditCard::RecordType::kVirtualCard);
    return copy;
  }
};

// Parameterized test class to test
// kAutofillEnableLogFormEventsToAllParsedFormTypes and ensure form event
// logging still works in the appropriate histograms when logging to parsed form
// types on a webpage.
class CreditCardFormEventLoggerTestWithParsedFormLogging
    : public CreditCardFormEventLoggerTest,
      public testing::WithParamInterface<bool> {
 public:
  CreditCardFormEventLoggerTestWithParsedFormLogging() {
    feature_list_.InitWithFeatureState(
        features::kAutofillEnableLogFormEventsToAllParsedFormTypes, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CreditCardFormEventLoggerTestWithParsedFormLogging,
                         testing::Bool());

// Tests that the `kBnplSuggestionAccepted` event is logged once when
// `OnDidAcceptBnplSuggestion()` is called.
TEST_F(CreditCardFormEventLoggerTest,
       OnDidAcceptBnplSuggestion_SuggestionAcceptedLogged) {
  base::HistogramTester histogram_tester;

  autofill_manager().GetCreditCardFormEventLogger().OnDidAcceptBnplSuggestion();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAccepted,
      /*expected_bucket_count=*/1);

  // Test that `kBnplSuggestionAccepted` is logged only once even if
  // `OnDidAcceptBnplSuggestion()` is called more than once on the same page.
  autofill_manager().GetCreditCardFormEventLogger().OnDidAcceptBnplSuggestion();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAccepted,
      /*expected_bucket_count=*/1);
}

// Tests that the Bnpl FormFilledOnce event is logged once when
// `OnDidFillFormFillingSuggestion()` is called after accepting a BNPL
// suggestion.
TEST_F(CreditCardFormEventLoggerTest,
       OnDidFillFormFillingSuggestion_BnplFormFilledOnce) {
  base::HistogramTester histogram_tester;

  FormStructure form =
      FormStructure(test::GetFormData({.fields = {{}, {}, {}}}));
  test_api(form).SetFieldTypes({CREDIT_CARD_EXP_MONTH,
                                CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                CREDIT_CARD_NUMBER});

  CreditCard card = test::GetVirtualCard();
  card.set_is_bnpl_card(true);
  card.set_issuer_id(kBnplAffirmIssuerId);

  auto on_did_fill_form_filling_suggestion = [&, this]() {
    autofill_manager()
        .GetCreditCardFormEventLogger()
        .OnDidFillFormFillingSuggestion(
            /*credit_card=*/card,
            /*form=*/form,
            /*field=*/AutofillField(),
            /*newly_filled_fields=*/base::flat_set<FieldGlobalId>(),
            /*safe_filled_fields=*/base::flat_set<FieldGlobalId>(),
            /*signin_state_for_metrics=*/
            AutofillMetrics::PaymentsSigninState::kSignedIn,
            /*trigger_source=*/AutofillTriggerSource::kPopup);
  };

  on_did_fill_form_filling_suggestion();
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kFormFilledWithAffirm,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/0),
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
                 /*count=*/0)));

  // Test that `kFormFilledWithAffirm` is logged only once even if
  // OnDidFillFormFillingSuggestion() is called more than once on the same page.
  on_did_fill_form_filling_suggestion();
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kFormFilledWithAffirm,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/0),
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
                 /*count=*/0)));
}

// Tests that the Bnpl FormFilledOnce event is not logged when
// `OnDidFillFormFillingSuggestion()` is called after accepting a non-BNPL
// suggestion.
TEST_F(CreditCardFormEventLoggerTest,
       OnDidFillFormFillingSuggestion_FormFilledOnce_NotBnpl) {
  base::HistogramTester histogram_tester;

  FormStructure form =
      FormStructure(test::GetFormData({.fields = {{}, {}, {}}}));
  test_api(form).SetFieldTypes({CREDIT_CARD_EXP_MONTH,
                                CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                CREDIT_CARD_NUMBER});

  autofill_manager()
      .GetCreditCardFormEventLogger()
      .OnDidFillFormFillingSuggestion(
          /*credit_card=*/test::GetVirtualCard(),
          /*form=*/form,
          /*field=*/AutofillField(),
          /*newly_filled_fields=*/base::flat_set<FieldGlobalId>(),
          /*safe_filled_fields=*/base::flat_set<FieldGlobalId>(),
          /*signin_state_for_metrics=*/
          AutofillMetrics::PaymentsSigninState::kSignedIn,
          /*trigger_source=*/AutofillTriggerSource::kPopup);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kFormFilledWithAffirm,
      /*expected_count=*/0);

  // Ensure that the regular VCN metrics are logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/1),
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
                 /*count=*/1)));
}

// Tests that `filled_credit_card_` is initialized when
// `OnDidFillFormFillingSuggestion()` is called with a BNPL issuer VCN.
TEST_F(CreditCardFormEventLoggerTest,
       OnDidAcceptBnplSuggestion_FilledCreditCardInitialized) {
  base::HistogramTester histogram_tester;

  FormStructure form =
      FormStructure(test::GetFormData({.fields = {{}, {}, {}}}));
  test_api(form).SetFieldTypes({CREDIT_CARD_EXP_MONTH,
                                CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                CREDIT_CARD_NUMBER});

  CreditCard card = test::GetVirtualCard();
  card.set_is_bnpl_card(true);
  card.set_issuer_id(kBnplAffirmIssuerId);

  EXPECT_FALSE(autofill_manager()
                   .GetCreditCardFormEventLogger()
                   .GetFilledCreditCardForTesting()
                   .has_value());

  autofill_manager()
      .GetCreditCardFormEventLogger()
      .OnDidFillFormFillingSuggestion(
          /*credit_card=*/card,
          /*form=*/form,
          /*field=*/AutofillField(),
          /*newly_filled_fields=*/base::flat_set<FieldGlobalId>(),
          /*safe_filled_fields=*/base::flat_set<FieldGlobalId>(),
          /*signin_state_for_metrics=*/
          AutofillMetrics::PaymentsSigninState::kSignedIn,
          /*trigger_source=*/AutofillTriggerSource::kPopup);

  ASSERT_TRUE(autofill_manager()
                  .GetCreditCardFormEventLogger()
                  .GetFilledCreditCardForTesting()
                  .has_value());
  EXPECT_TRUE(autofill_manager()
                  .GetCreditCardFormEventLogger()
                  .GetFilledCreditCardForTesting()
                  ->is_bnpl_card());
  EXPECT_EQ(autofill_manager()
                .GetCreditCardFormEventLogger()
                .GetFilledCreditCardForTesting()
                ->issuer_id(),
            kBnplAffirmIssuerId);
}

// Tests that `filled_credit_card_` is reset with the new card information when
// `OnDidFillFormFillingSuggestion()` is called again.
TEST_F(CreditCardFormEventLoggerTest,
       OnDidAcceptBnplSuggestion_FilledCreditCardReset) {
  base::HistogramTester histogram_tester;

  FormStructure form =
      FormStructure(test::GetFormData({.fields = {{}, {}, {}}}));
  test_api(form).SetFieldTypes({CREDIT_CARD_EXP_MONTH,
                                CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                CREDIT_CARD_NUMBER});

  CreditCard bnpl_card = test::GetVirtualCard();
  bnpl_card.set_is_bnpl_card(true);
  bnpl_card.set_issuer_id(kBnplAffirmIssuerId);

  auto on_did_fill_form_filling_suggestion = [&, this](CreditCard card) {
    autofill_manager()
        .GetCreditCardFormEventLogger()
        .OnDidFillFormFillingSuggestion(
            /*credit_card=*/card,
            /*form=*/form,
            /*field=*/AutofillField(),
            /*newly_filled_fields=*/base::flat_set<FieldGlobalId>(),
            /*safe_filled_fields=*/base::flat_set<FieldGlobalId>(),
            /*signin_state_for_metrics=*/
            AutofillMetrics::PaymentsSigninState::kSignedIn,
            /*trigger_source=*/AutofillTriggerSource::kPopup);
  };

  on_did_fill_form_filling_suggestion(bnpl_card);
  ASSERT_TRUE(autofill_manager()
                  .GetCreditCardFormEventLogger()
                  .GetFilledCreditCardForTesting()
                  .has_value());
  EXPECT_TRUE(autofill_manager()
                  .GetCreditCardFormEventLogger()
                  .GetFilledCreditCardForTesting()
                  ->is_bnpl_card());

  on_did_fill_form_filling_suggestion(test::GetVirtualCard());
  ASSERT_TRUE(autofill_manager()
                  .GetCreditCardFormEventLogger()
                  .GetFilledCreditCardForTesting()
                  .has_value());
  EXPECT_FALSE(autofill_manager()
                   .GetCreditCardFormEventLogger()
                   .GetFilledCreditCardForTesting()
                   ->is_bnpl_card());
}

// Tests that the Bnpl FormSubmittedOnce event is logged once when
// `LogFormSubmitted()` is called after filling a BNPL suggestion.
TEST_F(CreditCardFormEventLoggerTest, LogFormSubmitted_BnplFormFilledOnce) {
  base::HistogramTester histogram_tester;

  FormStructure form =
      FormStructure(test::GetFormData({.fields = {{}, {}, {}}}));
  test_api(form).SetFieldTypes({CREDIT_CARD_EXP_MONTH,
                                CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                CREDIT_CARD_NUMBER});

  CreditCard card = test::GetVirtualCard();
  card.set_is_bnpl_card(true);
  card.set_issuer_id(kBnplAffirmIssuerId);

  auto on_did_fill_form_filling_suggestion = [&, this]() {
    autofill_manager()
        .GetCreditCardFormEventLogger()
        .OnDidFillFormFillingSuggestion(
            /*credit_card=*/card,
            /*form=*/form,
            /*field=*/AutofillField(),
            /*newly_filled_fields=*/base::flat_set<FieldGlobalId>(),
            /*safe_filled_fields=*/base::flat_set<FieldGlobalId>(),
            /*signin_state_for_metrics=*/
            AutofillMetrics::PaymentsSigninState::kSignedIn,
            /*trigger_source=*/AutofillTriggerSource::kPopup);
  };

  on_did_fill_form_filling_suggestion();
  autofill_manager()
      .GetCreditCardFormEventLogger()
      .OnDidInteractWithAutofillableForm(form);
  autofill_manager().GetCreditCardFormEventLogger().OnWillSubmitForm(
      /*form=*/form);
  autofill_manager().GetCreditCardFormEventLogger().OnFormSubmitted(
      /*form=*/form);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kFormSubmittedWithAffirm,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                            /*count=*/0),
                     Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE,
                            /*count=*/0)));

  // Test that `kFormSubmittedWithAffirm` is logged only once even if
  // LogFormSubmitted() is called more than once on the same page.
  on_did_fill_form_filling_suggestion();
  autofill_manager()
      .GetCreditCardFormEventLogger()
      .OnDidInteractWithAutofillableForm(form);
  autofill_manager().GetCreditCardFormEventLogger().OnWillSubmitForm(
      /*form=*/form);
  autofill_manager().GetCreditCardFormEventLogger().OnFormSubmitted(
      /*form=*/form);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kFormSubmittedWithAffirm,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                            /*count=*/0),
                     Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE,
                            /*count=*/0)));
}

// Tests that the Bnpl FormSubmittedOnce event is not logged when
// `LogFormSubmitted()` is called after filling a non-BNPL VCN suggestion.
TEST_F(CreditCardFormEventLoggerTest,
       LogFormSubmitted_FormSubmittedOnce_NotBnpl) {
  base::HistogramTester histogram_tester;

  FormStructure form =
      FormStructure(test::GetFormData({.fields = {{}, {}, {}}}));
  test_api(form).SetFieldTypes({CREDIT_CARD_EXP_MONTH,
                                CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                CREDIT_CARD_NUMBER});

  autofill_manager()
      .GetCreditCardFormEventLogger()
      .OnDidFillFormFillingSuggestion(
          /*credit_card=*/test::GetVirtualCard(),
          /*form=*/form,
          /*field=*/AutofillField(),
          /*newly_filled_fields=*/base::flat_set<FieldGlobalId>(),
          /*safe_filled_fields=*/base::flat_set<FieldGlobalId>(),
          /*signin_state_for_metrics=*/
          AutofillMetrics::PaymentsSigninState::kSignedIn,
          /*trigger_source=*/AutofillTriggerSource::kPopup);
  autofill_manager()
      .GetCreditCardFormEventLogger()
      .OnDidInteractWithAutofillableForm(form);
  autofill_manager().GetCreditCardFormEventLogger().OnWillSubmitForm(
      /*form=*/form);
  autofill_manager().GetCreditCardFormEventLogger().OnFormSubmitted(
      /*form=*/form);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kFormSubmittedWithAffirm,
      /*expected_count=*/0);

  // Ensure that the regular VCN metrics are logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/1),
          Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
                 /*count=*/1)));
}

// Tests that the `kBnplSuggestionShown` event is logged once when
// `OnBnplSuggestionShown()` is called.
TEST_F(CreditCardFormEventLoggerTest,
       OnBnplSuggestionShown_SuggestionAddedLogged) {
  base::HistogramTester histogram_tester;

  autofill_manager().GetCreditCardFormEventLogger().OnBnplSuggestionShown();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionShown,
      /*expected_bucket_count=*/1);

  // Test that `kBnplSuggestionShown` is logged only once even if
  // `OnBnplSuggestionShown()` is called more than once on the same page.
  autofill_manager().GetCreditCardFormEventLogger().OnBnplSuggestionShown();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionShown,
      /*expected_bucket_count=*/1);
}

// Test that we log parsed form event for credit card forms.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardParsedFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Card Number", "card_number", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration", "cc_exp", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Verification", "verification", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                        CREDIT_CARD_EXP_MONTH,
                                        CREDIT_CARD_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// Test that events of standalone CVC forms are only logged to
// Autofill.FormEvents.StandaloneCvc and not to Autofill.FormEvents.CreditCard.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       StandaloneCvcParsedFormEvents) {
  FormData form = CreateForm({CreateTestFormField(
      "Standalone Cvc", "CVC", "", FormControlType::kInputText)});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  autofill_manager().AddSeenForm(form, field_types);

  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.StandaloneCvc",
                                      FORM_EVENT_DID_PARSE_FORM, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                     FORM_EVENT_DID_PARSE_FORM, 0);
}

// Test that we log interacted form event for credit cards related.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardInteractedFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the credit card field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.CreditCard",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardShownFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                       Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                       Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    // Suggestions not related to credit cards/addresses should not affect the
    // histograms.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kAutocompleteEntry);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsAre(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
                   Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0)));
  }
}

// Test that we log specific suggestion shown form events for virtual credit
// cards.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       VirtualCreditCardShownFormEvents) {
  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_VERIFICATION_CODE, CREDIT_CARD_NUMBER};

  // Creating cards, including a virtual card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card*/ true);

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    // Suggestions not related to credit cards/addresses should not affect the
    // histograms.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form,
                               /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kAutocompleteEntry);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0)));
  }

  // Recreate cards, this time *without* a virtual card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card*/ false);

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load. Suggestions shown should be
    // logged, but suggestions shown with virtual card should not.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD, 0),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE, 0)));
  }
}

// Test that we log selected form event for credit cards.
// TODO(crbug.com/362889813): Refactor the nested test cases into separate
// tests.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardSelectedFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Previewing suggestions should not record selected-form-events metrics.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kPreview, form, form.fields()[2].global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED, 0),
            Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED_ONCE, 0)));
  }

  {
    // Simulating selecting a local card suggestion multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }
  {
    // Simulating selecting a masked server card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a masked server card multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a virtual server suggestion by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424",
                                      /*is_virtual_card=*/true));
        });
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        &virtual_card, AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting a virtual card multiple times.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424",
                                      /*is_virtual_card=*/true));
        });
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        &virtual_card, AutofillTriggerSource::kPopup);
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424",
                                      /*is_virtual_card=*/true));
        });
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields()[2].global_id(),
        &virtual_card, AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 2),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  }
}

// Test that we log filled form events for credit cards.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardFilledFormEvents) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Disable mandatory reauth as it is not part of this test and will
  // interfere with the card retrieval flow.
  personal_data()
      .payments_data_manager()
      .SetPaymentMethodsMandatoryReauthEnabled(false);
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Previewing suggestions should not record filling-form-events metrics.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kPreview, form,
        form.fields().front().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 0),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 0)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a local card suggestion.
    base::HistogramTester histogram_tester;
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard);
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields().front().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a virtual card suggestion by selecting the option
    // based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424",
                                      /*is_virtual_card=*/true));
        });
    autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                                         form.fields().front().global_id(),
                                         &virtual_card,
                                         AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424"));
        });
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling multiple times.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields().front().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields().front().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
  }
}

// Test to log when an unique local card is autofilled, when other duplicated
// server and local cards exist.
TEST_P(
    CreditCardFormEventLoggerTestWithParsedFormLogging,
    CreditCardFilledFormEventsUsingUniqueLocalCardWhenOtherDuplicateServerCardsPresent) {
  // Clearing all the existing cards and creating a local credit card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  CreateLocalAndDuplicateServerCreditCard();
  std::string local_guid = kTestLocalCardId;

  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a unique local card suggestion.
  base::HistogramTester histogram_tester;
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, form.fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(local_guid),
      AutofillTriggerSource::kPopup);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE,
              0)));
}

// Test to log when a server card is autofilled and a local card with the same
// number exists.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardFilledFormEvents_UsingServerCard_WithLocalDuplicate) {
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  CreateLocalAndDuplicateServerCreditCard();
  std::string local_guid = kTestDuplicateMaskedCardId;
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a server card suggestion with a duplicate local card.
  base::HistogramTester histogram_tester;
  // Server card with a duplicate local card present at index 0.
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, form.fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(local_guid),
      AutofillTriggerSource::kPopup);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields().back().global_id());
  DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "5454545454545454");
  SubmitForm(form);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              1),
          Bucket(FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
                 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              1)));
}

// Test to log when a unique server card is autofilled and a different server
// card suggestion has the same number as a local card. That is, for local card
// A and server card B with the same number, this fills unrelated server card C.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardFilledFormEvents_UsingServerCard_WithoutLocalDuplicate) {
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  CreateLocalAndDuplicateServerCreditCard();
  std::string local_guid = kTestMaskedCardId;
  // Set up our form data.
  FormData form = test::GetFormData(
      {.description_for_logging = "PaymentProfileImportRequirements",
       .fields = {{.role = CREDIT_CARD_EXP_MONTH, .value = u""},
                  {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR, .value = u""},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);
  // Simulate filling a server card suggestion with a duplicate local card.
  base::HistogramTester histogram_tester;
  // Server card with a duplicate local card present at index 0.
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, form.fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(local_guid),
      AutofillTriggerSource::kPopup);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields().back().global_id());
  DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
  SubmitForm(form);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              0),
          Bucket(FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
                 0),
          Bucket(
              FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE,
              0)));
}

TEST_F(CreditCardFormEventLoggerTest,
       CreditCardSubmittedWithoutSelectingSuggestionsNoCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 1);
}

TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardSubmittedWithoutSelectingSuggestionsWrongSizeCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "411111111",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD, 1);
}

TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardSubmittedWithoutSelectingSuggestionsFailLuhnCheckCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "4444444444444444",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD, 1);
}

TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardSubmittedWithoutSelectingSuggestionsUnknownCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "5105105105105100",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD, 1);
}

TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardSubmittedWithoutSelectingSuggestionsKnownCard) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "4111111111111111",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 1);
}

TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       ShouldNotLogSubmitWithoutSelectingSuggestionsIfSuggestionFilled) {
  // Create a local card for testing, card number is 4111111111111111.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form = CreateForm(
      {CreateTestFormField("Month", "card_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Year", "card_year", "",
                           FormControlType::kInputText),
       CreateTestFormField("Credit card", "cardnum", "4111111111111111",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with suggestion shown and selected.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(
          kTestLocalCardId),
      AutofillTriggerSource::kPopup);

  SubmitForm(form);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD, 0),
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD,
                 0),
          Bucket(FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 0)));
}

// Test that we log submitted form events for credit cards.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardSubmittedFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1)));

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldValueChangedType::kHeuristicTypeName,
           CREDIT_CARD_NUMBER},
          {UkmTextFieldValueChangedType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldValueChangedType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown. Form is submitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    SubmitForm(form);
    // Trigger UploadFormDataAsyncCallback.
    test_api(autofill_client().GetAutofillDriverFactory())
        .Reset(autofill_driver());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1)));

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldValueChangedType::kHeuristicTypeName,
           CREDIT_CARD_NUMBER},
          {UkmTextFieldValueChangedType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldValueChangedType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields().front().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           base::to_underlying(CreditCard::RecordType::kLocalCard)},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields().front()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424",
                                      /*is_virtual_card=*/true));
        });
    autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                                         form.fields().front().global_id(),
                                         &virtual_card,
                                         AutofillTriggerSource::kPopup);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1)));

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           base::to_underlying(CreditCard::RecordType::kVirtualCard)},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields().front()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424"));
        });
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           base::to_underlying(CreditCard::RecordType::kMaskedServerCard)},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmSuggestionFilledType::kIsForCreditCardName, true},
          {UkmSuggestionFilledType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields().back()))
               .value()},
          {UkmSuggestionFilledType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    SubmitForm(form);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));

    VerifyUkm(
        &test_ukm_recorder(), form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldValueChangedType::kHeuristicTypeName,
           CREDIT_CARD_NUMBER},
          {UkmTextFieldValueChangedType::kHtmlFieldTypeName,
           HtmlFieldType::kUnspecified},
          {UkmTextFieldValueChangedType::kServerTypeName, CREDIT_CARD_NUMBER},
          {UkmSuggestionsShownType::kFieldSignatureName,
           Collapse(CalculateFieldSignatureForField(form.fields()[2])).value()},
          {UkmSuggestionsShownType::kFormSignatureName,
           Collapse(CalculateFormSignature(form)).value()}}});
  }
}

// Test that we log "will submit" and "submitted" form events for credit
// cards.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       CreditCardWillSubmitFormEvents) {
  // Creating all kinds of cards.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields().front().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled virtual card data by selecting the
    // option based on the enrolled masked card.
    base::HistogramTester histogram_tester;
    CreditCard virtual_card = GetVirtualCreditCard(kTestMaskedCardId);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424",
                                      /*is_virtual_card=*/true));
        });
    autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                                         form.fields().front().global_id(),
                                         &virtual_card,
                                         AutofillTriggerSource::kPopup);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce([](const CreditCard* card,
                     CreditCardAccessManager::OnCreditCardFetchedCallback cb) {
          std::move(cb).Run(BuildCard(u"6011000990139424"));
        });
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1)));
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/true);

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0)));
  }
}

// Test that we log form events for masked server card with offers.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       LogServerOfferFormEvents) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  // Creating all kinds of cards. None of them have offers.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  const auto kMaskedServerCardIds = std::to_array<std::string>({
      "12340000-0000-0000-0000-000000000001",
      "12340000-0000-0000-0000-000000000002",
      "12340000-0000-0000-0000-000000000003",
  });

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown and filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields().front().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                       Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1)));
    // Check that the offer sub-histogram was not recorded.
    // ExpectBucketCount() can't be used here because it expects the histogram
    // to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.WithOffer"]);

    // Ensure offers were not shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/0, 1);

    // Since no offers were shown, we should not track offer selection or
    // submission.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SelectedCardHasOffer"]);
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SubmittedCardHasOffer"]);
  }

  // Add another masked server card, this time with a linked offer.
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[0], "$4",
                                     autofill_driver_->url(),
                                     /*id=*/0x4fff);
  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the masked server card with the linked offer.
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[0]),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was selected and form was submitted with that card.
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SelectedCardHasOffer",
                                        /*sample=*/true, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/true, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion and
    // submitting the form. Verify that all related form events are correctly
    // logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select another card, and still log to offer
    // sub-histogram because user has another masked server card with offer.
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was not selected.
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SelectedCardHasOffer",
                                        /*sample=*/false, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/false, 1);
  }

  // Recreate cards and add card that is linked to an expired offer.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[1], "$4",
                                     autofill_driver_->url(),
                                     /*id=*/0x3fff, /*offer_expired=*/true);

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating activating the autofill popup for the credit card field,
    // new popup being shown and filling a local card suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the card with linked offer, though metrics should not record it
    // since the offer is expired.
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[1]),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
    SubmitForm(form);
    // Histograms without ".WithOffer" should be recorded.
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Check that the offer sub-histogram was not recorded.
    // ExpectBucketCount() can't be used here because it expects the
    // histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.WithOffer"]);

    // Ensure offers were not shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/0, 1);

    // Since no offers were shown, we should not track offer selection or
    // submission.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SelectedCardHasOffer"]);
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.Offer")["Autofill.Offer.SubmittedCardHasOffer"]);
  }

  // Recreate cards and add card that is linked to an offer.
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  AddMaskedServerCreditCardWithOffer(kMaskedServerCardIds[2], "$5",
                                     autofill_driver_->url(),
                                     /*id=*/0x5fff);

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, showing the
    // suggestions again, and then submitting the form with previously filled
    // card. Verify that all related form events are correctly logged to offer
    // sub-histogram. Making suggestions reappear tests confirmation of a fix
    // for crbug.com/1198751.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the masked server card with the linked offer.
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[2]),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");

    // Simulate user showing suggestions but then submitting form with
    // previously filled card info.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was selected and form was submitted with that card.
    histogram_tester.ExpectBucketCount("Autofill.Offer.SelectedCardHasOffer",
                                       /*sample=*/true, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/true, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // failing the CVC check and submitting the form anyways. Verify that all
    // related form events are correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    // Select the masked server card with the linked offer, but fail the CVC
    // check.
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[2]),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kPermanentFailure, std::string());

    // Submitting the form without the filled suggestion.
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Offer.SuggestedCardsHaveOffer",
        /*suggestions with offers=*/1, 1);

    // Should track card was selected once, but not submitted.
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SelectedCardHasOffer",
                                        /*sample=*/true, 1);
    histogram_tester.ExpectBucketCount("Autofill.Offer.SubmittedCardHasOffer",
                                       /*sample=*/true, 0);
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // A masked server card with linked offers.
    // Simulating activating the autofill popup for the credit card field, new
    // popup being shown, selecting a masked card server suggestion, but then
    // selecting a local card instead. Verify that all related form events are
    // correctly logged to offer sub-histogram.
    base::HistogramTester histogram_tester;

    // Show suggestions and select the card with offer.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kMaskedServerCardIds[2]),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");

    // Show suggestions again, and select a local card instead.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    DidShowAutofillSuggestions(form, /*field_index=*/form.fields().size() - 1,
                               SuggestionType::kCreditCardEntry);
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        personal_data().payments_data_manager().GetCreditCardByGUID(
            kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormEvents.CreditCard.WithOffer"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
            Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE, 1),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   1)));

    // Ensure we count the correct number of offers shown.
    histogram_tester.ExpectBucketCount("Autofill.Offer.SuggestedCardsHaveOffer",
                                       /*suggestions with offers=*/1, 1);

    // Should track card was only selected once.
    histogram_tester.ExpectBucketCount("Autofill.Offer.SelectedCardHasOffer",
                                       /*sample=*/true, 1);
    histogram_tester.ExpectBucketCount("Autofill.Offer.SelectedCardHasOffer",
                                       /*sample=*/false, 1);
    histogram_tester.ExpectUniqueSample("Autofill.Offer.SubmittedCardHasOffer",
                                        /*sample=*/false, 1);
  }
}

// Test that we log parsed form events for address and cards in the same form.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       MixedParsedFormEvents) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "", FormControlType::kInputText),
       CreateTestFormField("Card Number", "card_number", "",
                           FormControlType::kInputText),
       CreateTestFormField("Expiration", "cc_exp", "",
                           FormControlType::kInputText),
       CreateTestFormField("Verification", "verification", "",
                           FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE,          ADDRESS_HOME_CITY,
      ADDRESS_HOME_STREET_ADDRESS, CREDIT_CARD_NAME_FULL,
      CREDIT_CARD_EXP_MONTH,       CREDIT_CARD_VERIFICATION_CODE};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                      FORM_EVENT_DID_PARSE_FORM, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_DID_PARSE_FORM,
      1);
}

// A site can have two different <form> elements, one for an address and one
// for a credit card. It's common that only one of these forms receives a
// submit event, while the website actually submitted both. Test that
// the submit events are recorded for both of Autofill.FormEvents.{Address,
// CreditCard} after a submit event on the credit card form.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       SeparateCreditCardAndAddressForm_CreditCardSubmitted) {
  base::HistogramTester histogram_tester;
  FormData address_form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});
  FormData credit_card_form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "cardmonth", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  SeeForm(address_form);
  SeeForm(credit_card_form);
  // Show suggestions first as a prerequisite for
  // FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE gets logged.
  DidShowAutofillSuggestions(address_form, /*field_index=*/0,
                             SuggestionType::kAddressEntry);
  autofill_manager().OnAskForValuesToFillTest(
      address_form, address_form.fields().back().global_id());
  DidShowAutofillSuggestions(credit_card_form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(
      credit_card_form, credit_card_form.fields().back().global_id());
  SubmitForm(credit_card_form);

  size_t expected_address_count =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLogFormEventsToAllParsedFormTypes)
          ? 1
          : 0;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE,
                            expected_address_count),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE,
                            expected_address_count)));
}

// A site can have two different <form> elements, one for an address and one
// for a credit card. It's common that only one of these forms receives a
// submit event, while the website actually submitted both. Test that
// the submit events are recorded for both of Autofill.FormEvents.{Address,
// CreditCard} after a submit event on the Address form.
TEST_P(CreditCardFormEventLoggerTestWithParsedFormLogging,
       SeparateCreditCardAndAddressForm_AddressSubmitted) {
  base::HistogramTester histogram_tester;
  FormData address_form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});
  FormData credit_card_form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "cardmonth", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  SeeForm(address_form);
  SeeForm(credit_card_form);
  DidShowAutofillSuggestions(address_form, /*field_index=*/0,
                             SuggestionType::kAddressEntry);
  autofill_manager().OnAskForValuesToFillTest(
      address_form, address_form.fields().back().global_id());
  DidShowAutofillSuggestions(credit_card_form, /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().OnAskForValuesToFillTest(
      credit_card_form, credit_card_form.fields().back().global_id());
  SubmitForm(address_form);

  size_t expected_credit_card_count =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLogFormEventsToAllParsedFormTypes)
          ? 1
          : 0;
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE,
                            expected_credit_card_count),
                     Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE,
                            expected_credit_card_count)));
}

// Test that we log interacted form event for credit cards only once.
TEST_F(CreditCardFormEventLoggerTest, CreditCardFormEventsAreSegmented) {
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithNoData", FORM_EVENT_INTERACTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  test_api(autofill_client().GetAutofillDriverFactory())
      .Reset(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithBothServerAndLocalData",
        FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Tests that credit card form submissions are logged specially when the form is
// on a non-secure page.
TEST_F(CreditCardFormEventLoggerTest, NonSecureCreditCardForm) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Month", "cardmonth", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});
  std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  // Non-https origin.
  GURL frame_origin("http://example_root.com/form.html");
  form.set_main_frame_origin(url::Origin::Create(frame_origin));
  autofill_driver_->set_url(frame_origin);

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    SubmitForm(form);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  }
}

// Tests that credit card form submissions are *not* logged specially when the
// form is *not* on a non-secure page.
TEST_F(CreditCardFormEventLoggerTest,
       NonSecureCreditCardFormMetricsNotRecordedOnSecurePage) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  std::vector<FieldType> field_types = {CREDIT_CARD_NAME_FULL,
                                        CREDIT_CARD_NUMBER,
                                        CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    SubmitForm(form);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histograms.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                 FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  }
}

}  // namespace autofill::autofill_metrics
