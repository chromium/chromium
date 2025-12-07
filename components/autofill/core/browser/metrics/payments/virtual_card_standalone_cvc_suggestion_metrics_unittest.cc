// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_standalone_cvc_suggestion_metrics.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
    SetUpHelper();

    // Add a virtual card.
    card_ = test::WithCvc(test::GetVirtualCard(), u"123");
    card_.set_guid(kCardGuid);
    VirtualCardUsageData virtual_card_usage_data =
        test::GetVirtualCardUsageData1();
    card_.set_instrument_id(*virtual_card_usage_data.instrument_id());
    test_paydm().AddVirtualCardUsageData(virtual_card_usage_data);
    test_paydm().AddServerCreditCard(card_);

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
      BucketsAre(base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                  kStandaloneCvcSuggestionShown,
                              1),
                 base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
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
      BucketsAre(base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                  kStandaloneCvcSuggestionShown,
                              2),
                 base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
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
  autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form(),
                                       form().fields().front().global_id(),
                                       paydm().GetCreditCardByGUID(kCardGuid),
                                       AutofillTriggerSource::kPopup);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsAre(base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                  kStandaloneCvcSuggestionSelected,
                              1),
                 base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                  kStandaloneCvcSuggestionSelectedOnce,
                              1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED, 1),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE, 1)));

  // Simulate selecting the suggestion again.
  autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form(),
                                       form().fields().front().global_id(),
                                       paydm().GetCreditCardByGUID(kCardGuid),
                                       AutofillTriggerSource::kPopup);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsAre(base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                  kStandaloneCvcSuggestionSelected,
                              2),
                 base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
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
  EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(card()));
  autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form(),
                                       form().fields().front().global_id(),
                                       paydm().GetCreditCardByGUID(kCardGuid),
                                       AutofillTriggerSource::kPopup);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsInclude(base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                      kStandaloneCvcSuggestionFilled,
                                  1),
                     base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                      kStandaloneCvcSuggestionFilledOnce,
                                  1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.StandaloneCvc"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED, 1),
          base::Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE, 1)));

  // Fill the suggestion again.
  EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(card()));
  autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form(),
                                       form().fields().front().global_id(),
                                       paydm().GetCreditCardByGUID(kCardGuid),
                                       AutofillTriggerSource::kPopup);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsInclude(base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                      kStandaloneCvcSuggestionFilled,
                                  2),
                     base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
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
  EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(card()));
  autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form(),
                                       form().fields().front().global_id(),
                                       paydm().GetCreditCardByGUID(kCardGuid),
                                       AutofillTriggerSource::kPopup);
  SubmitForm(form());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.VirtualCard.StandaloneCvc.FormEvents"),
      BucketsInclude(base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
                                      kStandaloneCvcSuggestionWillSubmitOnce,
                                  1),
                     base::Bucket(VirtualCardStandaloneCvcSuggestionFormEvent::
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
