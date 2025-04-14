// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// TODO(crbug.com/409565194): Move `Autofill.FormEvents.CreditCard` tests from
// `autofill_metrics_unittest.cc` to
// `credit_card_form_event_logger_unittest.cc`.
class CreditCardFormEventLoggerTest : public AutofillMetricsBaseTest,
                                      public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

// Tests that the `kBnplSuggestionAcceptedOnce` event is logged once when
// `OnDidAcceptBnplSuggestion()` is called.
TEST_F(CreditCardFormEventLoggerTest,
       OnDidAcceptBnplSuggestion_SuggestionAcceptedLogged) {
  base::HistogramTester histogram_tester;

  autofill_manager().GetCreditCardFormEventLogger().OnDidAcceptBnplSuggestion();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAcceptedOnce,
      /*expected_bucket_count=*/1);

  // Test that `kBnplSuggestionAcceptedOnce` is logged only once even if
  // `OnDidAcceptBnplSuggestion()` is called more than once on the same page.
  autofill_manager().GetCreditCardFormEventLogger().OnDidAcceptBnplSuggestion();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAcceptedOnce,
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
      /*sample=*/autofill_metrics::BnplFormEvent::kFormFilledWithAffirmOnce,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      base::BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/0),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
                       /*count=*/0)));

  // Test that `kFormFilledWithAffirmOnce` is logged only once even if
  // OnDidFillFormFillingSuggestion() is called more than once on the same page.
  on_did_fill_form_filling_suggestion();
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kFormFilledWithAffirmOnce,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      base::BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/0),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
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
      /*sample=*/autofill_metrics::BnplFormEvent::kFormFilledWithAffirmOnce,
      /*expected_count=*/0);

  // Ensure that the regular VCN metrics are logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      base::BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/1),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
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
      /*sample=*/autofill_metrics::BnplFormEvent::kFormSubmittedWithAffirmOnce,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      base::BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                       /*count=*/0),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE,
                       /*count=*/0)));

  // Test that `kFormSubmittedWithAffirmOnce` is logged only once even if
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
      /*sample=*/autofill_metrics::BnplFormEvent::kFormSubmittedWithAffirmOnce,
      /*expected_count=*/1);

  // Ensure that BNPL VCN's don't affect regular VCN metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      base::BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                       /*count=*/0),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE,
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
      /*sample=*/autofill_metrics::BnplFormEvent::kFormSubmittedWithAffirmOnce,
      /*expected_count=*/0);

  // Ensure that the regular VCN metrics are logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      base::BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, /*count=*/1),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
                       /*count=*/1)));
}

// Tests that the `kBnplSuggestionShownOnce` event is logged once when
// `OnBnplSuggestionShown()` is called.
TEST_F(CreditCardFormEventLoggerTest,
       OnBnplSuggestionShown_SuggestionAddedLogged) {
  base::HistogramTester histogram_tester;

  autofill_manager().GetCreditCardFormEventLogger().OnBnplSuggestionShown();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionShownOnce,
      /*expected_bucket_count=*/1);

  // Test that `kBnplSuggestionShownOnce` is logged only once even if
  // `OnBnplSuggestionShown()` is called more than once on the same page.
  autofill_manager().GetCreditCardFormEventLogger().OnBnplSuggestionShown();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionShownOnce,
      /*expected_bucket_count=*/1);
}

}  // namespace autofill::autofill_metrics
