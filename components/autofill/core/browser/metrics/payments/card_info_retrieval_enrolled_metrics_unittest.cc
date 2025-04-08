// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_info_retrieval_enrolled_metrics.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

constexpr char kCardGuid[] = "10000000-0000-0000-0000-000000000001";

class CardInfoRetrievalEnrolledMetricsTest : public AutofillMetricsBaseTest,
                                             public testing::Test {
 public:
  CardInfoRetrievalEnrolledMetricsTest() = default;
  ~CardInfoRetrievalEnrolledMetricsTest() override = default;

  const FormData& form() const { return form_; }
  const CreditCard& card() const { return card_; }

  void SetUp() override {
    SetUpHelper();

    // Add a card info retrieval enrolled card.
    card_ =
        test::WithCvc(test::GetMaskedServerCardEnrolledIntoRuntimeRetrieval());
    card_.set_guid(kCardGuid);
    personal_data().test_payments_data_manager().AddServerCreditCard(card_);

    // Set up the form data. Reset form action to skip the IsFormMixedContent
    // check.
    form_ = GetAndAddSeenForm(
        {.description_for_logging = "CardInfoRetrievalEnrolled",
         .fields = {{.role = CREDIT_CARD_NAME_FULL},
                    {.role = CREDIT_CARD_NUMBER},
                    {.role = CREDIT_CARD_EXP_MONTH},
                    {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR}},
         .action = ""});
  }

  void TearDown() override { TearDownHelper(); }

 private:
  FormData form_;
  CreditCard card_;
};

// Test CVC suggestion shown metrics are correctly logged.
TEST_F(CardInfoRetrievalEnrolledMetricsTest, LogShownMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate activating the autofill popup for the CVC field.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FormEvents.CreditCard.CardInfoRetrievalEnrolled"),
      BucketsAre(
          base::Bucket(CardInfoRetrievalEnrolledLoggingEvent::kSuggestionShown,
                       1),
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionShownOnce, 1)));

  // Simulate activating the autofill popup for the CVC field again.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FormEvents.CreditCard.CardInfoRetrievalEnrolled"),
      BucketsAre(
          base::Bucket(CardInfoRetrievalEnrolledLoggingEvent::kSuggestionShown,
                       2),
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionShownOnce, 1)));
}

// Test CVC suggestion selected metrics are correctly logged.
TEST_F(CardInfoRetrievalEnrolledMetricsTest, LogSelectedMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate selecting the CVC suggestion.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form(),
      form().fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      AutofillTriggerSource::kPopup);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FormEvents.CreditCard.CardInfoRetrievalEnrolled"),
      BucketsAre(
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionSelected, 1),
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionSelectedOnce,
              1)));

  // Simulate selecting the suggestion again.
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form(),
      form().fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      AutofillTriggerSource::kPopup);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FormEvents.CreditCard.CardInfoRetrievalEnrolled"),
      BucketsAre(
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionSelected, 2),
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionSelectedOnce,
              1)));
}

// Test CVC suggestion filled metrics are correctly logged.
TEST_F(CardInfoRetrievalEnrolledMetricsTest, LogFilledMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate filling the CVC suggestion.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(card()));
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form(),
      form().fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      AutofillTriggerSource::kPopup);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FormEvents.CreditCard.CardInfoRetrievalEnrolled"),
      BucketsInclude(
          base::Bucket(CardInfoRetrievalEnrolledLoggingEvent::kSuggestionFilled,
                       1),
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionFilledOnce,
              1)));

  // Fill the suggestion again.
  EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(card()));
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form(),
      form().fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      AutofillTriggerSource::kPopup);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FormEvents.CreditCard.CardInfoRetrievalEnrolled"),
      BucketsInclude(
          base::Bucket(CardInfoRetrievalEnrolledLoggingEvent::kSuggestionFilled,
                       2),
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionFilledOnce,
              1)));
}

// Test will submit and submitted metrics are correctly logged.
TEST_F(CardInfoRetrievalEnrolledMetricsTest, LogSubmitMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate filling and then submitting the card.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(card()));
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form(),
      form().fields().front().global_id(),
      personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      AutofillTriggerSource::kPopup);
  SubmitForm(form());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FormEvents.CreditCard.CardInfoRetrievalEnrolled"),
      BucketsInclude(
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionWillSubmitOnce,
              1),
          base::Bucket(
              CardInfoRetrievalEnrolledLoggingEvent::kSuggestionSubmittedOnce,
              1)));
}

}  // namespace autofill::autofill_metrics
