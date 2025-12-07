// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

using IssuerId = autofill::BnplIssuer::IssuerId;

// Params of the BnplMetricsTest:
// -- std::string_view issuer_id;
class BnplMetricsTest : public AutofillMetricsBaseTest,
                        public testing::Test,
                        public testing::WithParamInterface<IssuerId> {
 public:
  BnplMetricsTest() = default;
  ~BnplMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

  IssuerId GetIssuerId() { return GetParam(); }
};

// BNPL is currently only available for desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Test that we log when the user flips the BNPL enabled toggle.
TEST_F(BnplMetricsTest, LogBnplPrefToggled) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(autofill_client().GetPrefs()->GetBoolean(
      autofill::prefs::kAutofillBnplEnabled));
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     0);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     0);

  autofill_client().GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillBnplEnabled, false);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     0);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     1);

  autofill_client().GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillBnplEnabled, true);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     1);
}

TEST_F(BnplMetricsTest, LogBnplIssuersSyncedCountAtStartup) {
  base::HistogramTester histogram_tester;

  int count = 5;
  LogBnplIssuersSyncedCountAtStartup(count);
  histogram_tester.ExpectBucketCount("Autofill.Bnpl.IssuersSyncedCount.Startup",
                                     count, 1);

  count = 25;
  LogBnplIssuersSyncedCountAtStartup(count);
  histogram_tester.ExpectBucketCount("Autofill.Bnpl.IssuersSyncedCount.Startup",
                                     count, 1);

  histogram_tester.ExpectTotalCount("Autofill.Bnpl.IssuersSyncedCount.Startup",
                                    2);
}

TEST_P(BnplMetricsTest, LogBnplTosDialogShown) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplTosDialogShown(issuer_id);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.TosDialogShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(BnplMetricsTest,
       LogBnplSuggestionUnavailableReason_AmountExtractionFailure) {
  base::HistogramTester histogram_tester;

  LogBnplSuggestionUnavailableReason(
      BnplSuggestionUnavailableReason::kAmountExtractionFailure);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      BnplSuggestionUnavailableReason::kAmountExtractionFailure, 1);
}

TEST_F(BnplMetricsTest,
       LogBnplSuggestionUnavailableReason_CheckoutAmountNotSupported) {
  base::HistogramTester histogram_tester;

  LogBnplSuggestionUnavailableReason(
      BnplSuggestionUnavailableReason::kCheckoutAmountNotSupported);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      BnplSuggestionUnavailableReason::kCheckoutAmountNotSupported, 1);
}

TEST_F(BnplMetricsTest,
       BnplSuggestionUnavailableReason_AmountExtractionTimeout) {
  base::HistogramTester histogram_tester;

  LogBnplSuggestionUnavailableReason(
      BnplSuggestionUnavailableReason::kAmountExtractionTimeout);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      BnplSuggestionUnavailableReason::kAmountExtractionTimeout, 1);
}

TEST_F(BnplMetricsTest, LogSelectBnplIssuerDialogResult_Cancelled) {
  base::HistogramTester histogram_tester;
  LogSelectBnplIssuerDialogResult(
      SelectBnplIssuerDialogResult::kCancelButtonClicked);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SelectionDialogResult",
      SelectBnplIssuerDialogResult::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

TEST_F(BnplMetricsTest, LogSelectBnplIssuerDialogResult_IssuerSelected) {
  base::HistogramTester histogram_tester;
  LogSelectBnplIssuerDialogResult(
      SelectBnplIssuerDialogResult::kIssuerSelected);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SelectionDialogResult",
      SelectBnplIssuerDialogResult::kIssuerSelected,
      /*expected_bucket_count=*/1);
}

TEST_P(BnplMetricsTest, LogBnplIssuerSelection) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplIssuerSelection(issuer_id);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SelectionDialogIssuerSelected", issuer_id,
      /*expected_bucket_count=*/1);
}

TEST_F(BnplMetricsTest, LogBnplAddedOnUpdateSuggestion) {
  base::HistogramTester histogram_tester;

  LogBnplFormEvent(BnplFormEvent::kBnplSuggestionShown);

  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard.Bnpl",
                                     BnplFormEvent::kBnplSuggestionShown, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowShown) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplPopupWindowShown(issuer_id);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowResult_Success) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplPopupWindowResult(issuer_id, BnplFlowResult::kSuccess);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplFlowResult::kSuccess, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowResult_Failure) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplPopupWindowResult(issuer_id, BnplFlowResult::kFailure);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplFlowResult::kFailure, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowResult_UserClosed) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplPopupWindowResult(issuer_id, BnplFlowResult::kUserClosed);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplFlowResult::kUserClosed, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowLatency_Success) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplPopupWindowLatency(base::Milliseconds(1000), issuer_id,
                            BnplFlowResult::kSuccess);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowLatency.",
                    GetHistogramSuffixFromIssuerId(issuer_id), ".",
                    ConvertBnplFlowResultToString(BnplFlowResult::kSuccess)}),
      1000, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowLatency_Failure) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplPopupWindowLatency(base::Milliseconds(2000), issuer_id,
                            BnplFlowResult::kFailure);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowLatency.",
                    GetHistogramSuffixFromIssuerId(issuer_id), ".",
                    ConvertBnplFlowResultToString(BnplFlowResult::kFailure)}),
      2000, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowLatency_UserClosed) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplPopupWindowLatency(base::Milliseconds(3000), issuer_id,
                            BnplFlowResult::kUserClosed);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Autofill.Bnpl.PopupWindowLatency.",
           GetHistogramSuffixFromIssuerId(issuer_id), ".",
           ConvertBnplFlowResultToString(BnplFlowResult::kUserClosed)}),
      3000, 1);
}

TEST_P(BnplMetricsTest, LogBnplTosDialogResult_AcceptButtonClicked) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplTosDialogResult(BnplTosDialogResult::kAcceptButtonClicked, issuer_id);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.TosDialogResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplTosDialogResult::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

TEST_P(BnplMetricsTest, LogBnplTosDialogResult_CancelButtonClicked) {
  base::HistogramTester histogram_tester;
  IssuerId issuer_id = GetIssuerId();

  LogBnplTosDialogResult(BnplTosDialogResult::kCancelButtonClicked, issuer_id);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.TosDialogResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplTosDialogResult::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

TEST_F(BnplMetricsTest, LogBnplSelectionDialogShown) {
  base::HistogramTester histogram_tester;

  LogBnplSelectionDialogShown();
  histogram_tester.ExpectUniqueSample("Autofill.Bnpl.SelectionDialogShown",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(,
                         BnplMetricsTest,
                         testing::Values(IssuerId::kBnplAffirm,
                                         IssuerId::kBnplZip,
                                         IssuerId::kBnplAfterpay,
                                         IssuerId::kBnplKlarna));

class BnplFormEventsMetricsTest : public AutofillMetricsBaseTest,
                                  public testing::Test {
 public:
  BnplFormEventsMetricsTest() = default;
  FormData form() { return form_; }

  void SetUp() override {
    SetUpHelper();

    form_ =
        GetAndAddSeenForm({.description_for_logging = "Bnpl",
                           .fields = {{.role = CREDIT_CARD_NAME_FULL},
                                      {.role = CREDIT_CARD_NUMBER},
                                      {.role = CREDIT_CARD_EXP_MONTH},
                                      {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR}},
                           .action = ""});

    test_paydm().AddBnplIssuer(test::GetTestLinkedBnplIssuer());
  }

  void TearDown() override { TearDownHelper(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnableBuyNowPayLaterSyncing};
  FormData form_;
};

TEST_F(BnplFormEventsMetricsTest, SuggestionsShownOnBnplEligibleMerchant) {
  base::HistogramTester histogram_tester;

  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().back().global_id());

  ON_CALL(
      *static_cast<MockAutofillOptimizationGuideDecider*>(
          autofill_manager().client().GetAutofillOptimizationGuideDecider()),
      IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields().size() - 1,
                             SuggestionType::kCreditCardEntry);

  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard.Bnpl",
                                     BnplFormEvent::kSuggestionsShown, 1);

  // To ensure the metrics logs only once per page.
  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields().size() - 1,
                             SuggestionType::kCreditCardEntry);

  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard.Bnpl",
                                     BnplFormEvent::kSuggestionsShown, 1);
}

TEST_F(BnplFormEventsMetricsTest, BnplSuggestionsNotShownDueToUrl) {
  base::HistogramTester histogram_tester;

  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().back().global_id());

  ON_CALL(
      *static_cast<MockAutofillOptimizationGuideDecider*>(
          autofill_manager().client().GetAutofillOptimizationGuideDecider()),
      IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(false));

  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields().size() - 1,
                             SuggestionType::kCreditCardEntry);

  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard.Bnpl",
                                     BnplFormEvent::kSuggestionsShown, 0);
}

TEST_F(BnplFormEventsMetricsTest, SuggestionAccepted) {
  base::HistogramTester histogram_tester;

  LogBnplFormEvent(BnplFormEvent::kBnplSuggestionAccepted);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kBnplSuggestionAccepted,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormFilledOnceWithAffirm) {
  base::HistogramTester histogram_tester;

  LogFormFilledWithBnplVcn(BnplIssuer::IssuerId::kBnplAffirm);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormFilledWithAffirm,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormFilledOnceWithZip) {
  base::HistogramTester histogram_tester;

  LogFormFilledWithBnplVcn(BnplIssuer::IssuerId::kBnplZip);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormFilledWithZip,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormFilledOnceWithKlarna) {
  base::HistogramTester histogram_tester;

  LogFormFilledWithBnplVcn(BnplIssuer::IssuerId::kBnplKlarna);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormFilledWithKlarna,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormFilledOnceWithAfterpay) {
  base::HistogramTester histogram_tester;

  LogFormFilledWithBnplVcn(BnplIssuer::IssuerId::kBnplAfterpay);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormFilledWithAfterpay,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormSubmittedOnceWithAffirm) {
  base::HistogramTester histogram_tester;

  LogFormSubmittedWithBnplVcn(BnplIssuer::IssuerId::kBnplAffirm);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormSubmittedWithAffirm,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormSubmittedOnceWithZip) {
  base::HistogramTester histogram_tester;

  LogFormSubmittedWithBnplVcn(BnplIssuer::IssuerId::kBnplZip);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormSubmittedWithZip,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormSubmittedOnceWithKlarna) {
  base::HistogramTester histogram_tester;

  LogFormSubmittedWithBnplVcn(BnplIssuer::IssuerId::kBnplKlarna);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormSubmittedWithKlarna,
      /*expected_count=*/1);
}

TEST_F(BnplFormEventsMetricsTest, FormSubmittedOnceWithAfterpay) {
  base::HistogramTester histogram_tester;

  LogFormSubmittedWithBnplVcn(BnplIssuer::IssuerId::kBnplAfterpay);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/BnplFormEvent::kFormSubmittedWithAfterpay,
      /*expected_count=*/1);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::autofill_metrics
