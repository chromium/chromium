// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/cvc_storage_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"

namespace autofill::autofill_metrics {

namespace {
constexpr char kCardGuid[] = "10000000-0000-0000-0000-000000000001";
}  // namespace

// Enum class for different CVC form types, which is used to parameterize the
// metric test.
enum class CvcFormType {
  kNormalCreditCardForm,
  // A single field form with CREDIT_CARD_VERIFICATION_CODE.
  kStandaloneCvcWithLegacyVerificationCodeField,
  // A single field form with CREDIT_CARD_STANDALONE_VERIFICATION_CODE.
  kStandaloneCvcForm,
};

// Params of CvcStorageMetricsTest:
// -- CvcFormType form_type: Indicates which type of CVC form.
// -- bool using_local_card: Indicates which type of card is used. If true, use
// local card. Otherwise, use masked server card.
class CvcStorageMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test,
      public testing::WithParamInterface<std::tuple<CvcFormType, bool>> {
 public:
  CvcStorageMetricsTest() = default;
  ~CvcStorageMetricsTest() override = default;

  const FormData& form() const { return form_; }
  const CreditCard& card() const { return card_; }
  bool using_local_card() const { return std::get<1>(GetParam()); }

  void SetUp() override {
    SetUpHelper();
    form_type_ = std::get<0>(GetParam());

    // Set up the form data. Reset form action to skip the IsFormMixedContent
    // check.
    form_ = GetAndAddSeenForm({.description_for_logging = "CvcStorage",
                               .fields = GetTestFormDataFields(),
                               .action = ""});

    if (using_local_card()) {
      card_ = test::WithCvc(test::GetCreditCard(), /*cvc=*/u"789");
      card_.set_guid(kCardGuid);
      personal_data().test_payments_data_manager().AddCreditCard(card_);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
      // Disable mandatory reauth as it is not part of this test and will
      // interfere with the card retrieval flow.
      autofill_client_->GetPrefs()->SetBoolean(
          prefs::kAutofillPaymentMethodsMandatoryReauth, false);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_IOS)
    } else {
      // Add a masked server card.
      card_ = test::WithCvc(test::GetMaskedServerCard());
      card_.set_guid(kCardGuid);
      personal_data().test_payments_data_manager().AddServerCreditCard(card_);
    }
    test_api(autofill_manager())
        .SetFourDigitCombinationsInDOM(
            {base::UTF16ToUTF8(card_.LastFourDigits())});
  }

  void TearDown() override { TearDownHelper(); }

  std::vector<test::FieldDescription> GetTestFormDataFields() {
    switch (form_type_) {
      case CvcFormType::kNormalCreditCardForm:
        return {{.role = CREDIT_CARD_NAME_FULL},
                {.role = CREDIT_CARD_NUMBER},
                {.role = CREDIT_CARD_EXP_MONTH},
                {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR},
                {.role = CREDIT_CARD_VERIFICATION_CODE}};
      case CvcFormType::kStandaloneCvcWithLegacyVerificationCodeField:
        return {{.role = CREDIT_CARD_VERIFICATION_CODE}};
      case CvcFormType::kStandaloneCvcForm:
        return {{.role = CREDIT_CARD_STANDALONE_VERIFICATION_CODE}};
    }
  }

  // Return the histogram name which should be logged to.
  std::string GetExpectedHistogramName() {
    switch (form_type_) {
      case CvcFormType::kNormalCreditCardForm:
        return "Autofill.FormEvents.CreditCard";
      case CvcFormType::kStandaloneCvcWithLegacyVerificationCodeField:
      case CvcFormType::kStandaloneCvcForm:
        return "Autofill.FormEvents.StandaloneCvc";
    }
  }

  // Return the histogram name which should NOT be logged to.
  std::string GetHistogramNameForEmptyRecord() {
    switch (form_type_) {
      case CvcFormType::kNormalCreditCardForm:
        return "Autofill.FormEvents.StandaloneCvc";
      case CvcFormType::kStandaloneCvcWithLegacyVerificationCodeField:
      case CvcFormType::kStandaloneCvcForm:
        return "Autofill.FormEvents.CreditCard";
    }
  }

 private:
  CreditCard card_;
  FormData form_;
  CvcFormType form_type_;
};

// Test CVC suggestion shown metrics are correctly logged.
TEST_P(CvcStorageMetricsTest, LogShownMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /* enabled_features */
      {features::kAutofillEnableCvcStorageAndFilling,
       features::kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement},
      /* disabled_features */ {});
  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate activating the autofill popup for the credit card field.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetExpectedHistogramName()),
      BucketsInclude(
          base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN_ONCE, 1)));

  // Show the popup again.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetExpectedHistogramName()),
      BucketsInclude(
          base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN_ONCE, 1)));
  histogram_tester.ExpectTotalCount(GetHistogramNameForEmptyRecord(), 0);
}

// Test CVC suggestion selected metrics are correctly logged.
TEST_P(CvcStorageMetricsTest, LogSelectedMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate selecting the suggestion with CVC.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().back().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields().size() - 1,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().back(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetExpectedHistogramName()),
      BucketsInclude(
          base::Bucket(using_local_card()
                           ? FORM_EVENT_LOCAL_SUGGESTION_FILLED
                           : FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                       1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED_ONCE,
                       1)));

  // Simulate selecting the suggestion again.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetExpectedHistogramName()),
      BucketsInclude(
          base::Bucket(using_local_card()
                           ? FORM_EVENT_LOCAL_SUGGESTION_FILLED
                           : FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                       2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED_ONCE,
                       1)));
  histogram_tester.ExpectTotalCount(GetHistogramNameForEmptyRecord(), 0);
}

// Test CVC suggestion filled metrics are correctly logged.
TEST_P(CvcStorageMetricsTest, LogFilledMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate filling the suggestion with CVC.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  if (!using_local_card()) {
    test_api(autofill_manager())
        .OnCreditCardFetched(form(), form().fields().front(),
                             AutofillTriggerSource::kPopup,
                             CreditCardFetchResult::kSuccess, &card());
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetExpectedHistogramName()),
      BucketsInclude(
          base::Bucket(using_local_card()
                           ? FORM_EVENT_LOCAL_SUGGESTION_FILLED
                           : FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED,
                       1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED_ONCE,
                       1)));

  // Fill the suggestion again.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  if (!using_local_card()) {
    test_api(autofill_manager())
        .OnCreditCardFetched(form(), form().fields().front(),
                             AutofillTriggerSource::kPopup,
                             CreditCardFetchResult::kSuccess, &card());
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetExpectedHistogramName()),
      BucketsInclude(
          base::Bucket(using_local_card()
                           ? FORM_EVENT_LOCAL_SUGGESTION_FILLED
                           : FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED,
                       2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED_ONCE,
                       1)));
  histogram_tester.ExpectTotalCount(GetHistogramNameForEmptyRecord(), 0);
}

// Test will submit and submitted metrics are correctly logged.
TEST_P(CvcStorageMetricsTest, LogSubmitMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate filling and then submitting the card with CVC.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  if (!using_local_card()) {
    test_api(autofill_manager())
        .OnCreditCardFetched(form(), form().fields().front(),
                             AutofillTriggerSource::kPopup,
                             CreditCardFetchResult::kSuccess, &card());
  }
  SubmitForm(form());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetExpectedHistogramName()),
      BucketsInclude(
          base::Bucket(
              using_local_card()
                  ? FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE
                  : FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
              1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_WILL_SUBMIT_ONCE,
                       1),
          base::Bucket(
              using_local_card()
                  ? FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE
                  : FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
              1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SUBMITTED_ONCE,
                       1)));
  histogram_tester.ExpectTotalCount(GetHistogramNameForEmptyRecord(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CvcStorageMetricsTest,
    testing::Combine(
        testing::Values(
            CvcFormType::kNormalCreditCardForm,
            CvcFormType::kStandaloneCvcWithLegacyVerificationCodeField,
            CvcFormType::kStandaloneCvcForm),
        testing::Bool()));

}  // namespace autofill::autofill_metrics
