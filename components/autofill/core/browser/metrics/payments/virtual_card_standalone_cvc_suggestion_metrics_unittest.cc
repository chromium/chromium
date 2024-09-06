// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_standalone_cvc_suggestion_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

constexpr char kCardGuid[] = "10000000-0000-0000-0000-000000000001";

class VirtualCardStandaloneCvcMetricsTest : public AutofillMetricsBaseTest,
                                            public testing::Test {
 public:
  VirtualCardStandaloneCvcMetricsTest() = default;
  ~VirtualCardStandaloneCvcMetricsTest() override = default;

  const FormData& form() const { return form_; }
  const CreditCard& card() const { return card_; }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillParseVcnCardOnFileStandaloneCvcFields);
    SetUpHelper();

    // Add a virtual card.
    card_ = test::WithCvc(test::GetVirtualCard(), u"123");
    card_.set_guid(kCardGuid);
    VirtualCardUsageData virtual_card_usage_data =
        test::GetVirtualCardUsageData1();
    card_.set_instrument_id(*virtual_card_usage_data.instrument_id());
    personal_data().test_payments_data_manager().AddVirtualCardUsageData(
        virtual_card_usage_data);
    personal_data().test_payments_data_manager().AddServerCreditCard(card_);

    // Set four_digit_combinations_in_dom_ to simulate the list of last four
    // digits detected from the origin webpage.
    test_api(autofill_manager()).SetFourDigitCombinationsInDOM({"1234"});
    // Set up the form data. Reset form action to skip the IsFormMixedContent
    // check.
    form_ = GetAndAddSeenForm(
        {.description_for_logging = "StandaloneCvc",
         .fields = {{.role = CREDIT_CARD_STANDALONE_VERIFICATION_CODE}},
         .action = "",
         .main_frame_origin = virtual_card_usage_data.merchant_origin()});
  }

  void TearDown() override { TearDownHelper(); }

 private:
  FormData form_;
  CreditCard card_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test CVC suggestion shown metrics are correctly logged.
TEST_F(VirtualCardStandaloneCvcMetricsTest, LogShownMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate activating the autofill popup for the CVC field.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionShown,
              1),
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionShownOnce,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                     base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));

  // Simulate activating the autofill popup for the CVC field again.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionShown,
              2),
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionShownOnce,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                     base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
  histogram_tester.ExpectTotalCount("Autofill.FormEvents.CreditCard", 0);
}

// Test CVC suggestion selected metrics are correctly logged.
TEST_F(VirtualCardStandaloneCvcMetricsTest, LogSelectedMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate selecting the CVC suggestion.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionSelected,
              1),
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionSelectedOnce,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 1),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));

  // Simulate selecting the suggestion again.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionSelected,
              2),
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionSelectedOnce,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 2),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));
  histogram_tester.ExpectTotalCount("Autofill.FormEvents.CreditCard", 0);
}

// Test CVC suggestion filled metrics are correctly logged.
TEST_F(VirtualCardStandaloneCvcMetricsTest, LogFilledMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate filling the CVC suggestion.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(form(), form().fields().front(),
                           AutofillTriggerSource::kPopup,
                           CreditCardFetchResult::kSuccess, &card());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsInclude(
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionFilled,
              1),
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionFilledOnce,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 1),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1)));

  // Fill the suggestion again.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(form(), form().fields().front(),
                           AutofillTriggerSource::kPopup,
                           CreditCardFetchResult::kSuccess, &card());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsInclude(
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionFilled,
              2),
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionFilledOnce,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 2),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1)));
  histogram_tester.ExpectTotalCount("Autofill.FormEvents.CreditCard", 0);
}

// Test will submit and submitted metrics are correctly logged.
TEST_F(VirtualCardStandaloneCvcMetricsTest, LogSubmitMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate filling and then submitting the card.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(form(), form().fields().front(),
                           AutofillTriggerSource::kPopup,
                           CreditCardFetchResult::kSuccess, &card());
  SubmitForm(form());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsInclude(
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionWillSubmitOnce,
              1),
          base::Bucket(
              autofill_metrics::VirtualCardStandaloneCvcSuggestionFormEvent::
                  kStandaloneCvcSuggestionSubmittedOnce,
              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE, 1)));
  // Verify that not logging to normal credit card UMA.
  histogram_tester.ExpectTotalCount("Autofill.FormEvents.CreditCard", 0);
}

}  // namespace
}  // namespace autofill::autofill_metrics
