// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// Params of the BnplMetricsTest:
// -- std::string_view issuer_id;
class BnplMetricsTest : public AutofillMetricsBaseTest,
                        public testing::Test,
                        public testing::WithParamInterface<std::string_view> {
 public:
  BnplMetricsTest() = default;
  ~BnplMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

  std::string_view GetBnplIssuerId() { return GetParam(); }
};

// BNPL is currently only available for desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Test that we log when the user flips the BNPL enabled toggle.
TEST_F(BnplMetricsTest, LogBnplPrefToggled) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(autofill_client_->GetPrefs()->GetBoolean(
      autofill::prefs::kAutofillBnplEnabled));
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     0);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     0);

  autofill_client_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillBnplEnabled, false);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     0);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     1);

  autofill_client_->GetPrefs()->SetBoolean(
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
  std::string_view issuer_id = GetBnplIssuerId();

  LogBnplTosDialogShown(issuer_id);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.TosDialogShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(BnplMetricsTest,
       LogBnplSuggestionNotShownReason_AmountExtractionFailure) {
  base::HistogramTester histogram_tester;

  LogBnplSuggestionNotShownReason(
      BnplSuggestionNotShownReason::kAmountExtractionFailure);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      BnplSuggestionNotShownReason::kAmountExtractionFailure, 1);
}

TEST_F(BnplMetricsTest,
       LogBnplSuggestionNotShownReason_CheckoutAmountNotSupported) {
  base::HistogramTester histogram_tester;

  LogBnplSuggestionNotShownReason(
      BnplSuggestionNotShownReason::kCheckoutAmountNotSupported);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      BnplSuggestionNotShownReason::kCheckoutAmountNotSupported, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowShown) {
  base::HistogramTester histogram_tester;
  std::string_view issuer_id = GetBnplIssuerId();

  LogBnplPopupWindowShown(issuer_id);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowResult_Success) {
  base::HistogramTester histogram_tester;
  std::string_view issuer_id = GetBnplIssuerId();

  LogBnplPopupWindowResult(issuer_id, BnplFlowResult::kSuccess);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplFlowResult::kSuccess, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowResult_Failure) {
  base::HistogramTester histogram_tester;
  std::string_view issuer_id = GetBnplIssuerId();

  LogBnplPopupWindowResult(issuer_id, BnplFlowResult::kFailure);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplFlowResult::kFailure, 1);
}

TEST_P(BnplMetricsTest, LogBnplPopupWindowResult_UserClosed) {
  base::HistogramTester histogram_tester;
  std::string_view issuer_id = GetBnplIssuerId();

  LogBnplPopupWindowResult(issuer_id, BnplFlowResult::kUserClosed);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)}),
      BnplFlowResult::kUserClosed, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         BnplMetricsTest,
                         testing::Values(kBnplAffirmIssuerId,
                                         kBnplZipIssuerId,
                                         kBnplAfterpayIssuerId));

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::autofill_metrics
