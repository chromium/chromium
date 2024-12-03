// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

TEST(FacilitatedPaymentsMetricsTest, LogPixCodeCopied) {
  base::HistogramTester histogram_tester;

  LogPixCodeCopied(ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.PixCodeCopied",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogFopSelected) {
  base::HistogramTester histogram_tester;

  LogFopSelected();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelector.UserAction",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_ValidatorFailed) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(
      /*result=*/base::unexpected("Data Decoder terminated unexpectedly"),
      base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.ValidatorFailed.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_InvalidCode) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(/*result=*/false,
                                           base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.InvalidCode.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogPaymentCodeValidationResultAndLatency_ValidCode) {
  base::HistogramTester histogram_tester;

  LogPaymentCodeValidationResultAndLatency(/*result=*/true,
                                           base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.ValidCode.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogApiAvailabilityCheckResultAndLatency) {
  base::HistogramTester histogram_tester;

  LogApiAvailabilityCheckResultAndLatency(/*result=*/true,
                                          base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Success.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);

  LogApiAvailabilityCheckResultAndLatency(/*result=*/false,
                                          base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Failure.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogLoadRiskDataResult) {
  base::HistogramTester histogram_tester;

  LogLoadRiskDataResultAndLatency(/*was_successful=*/true,
                                  base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.LoadRiskData.Success.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogGetClientTokenResultAndLatency) {
  base::HistogramTester histogram_tester;

  LogGetClientTokenResultAndLatency(/*result=*/true, base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.GetClientToken.Success.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogInitiatePaymentAttempt) {
  base::HistogramTester histogram_tester;

  LogInitiatePaymentAttempt();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogInitiatePaymentResultAndLatency) {
  for (bool result : {true, false}) {
    base::HistogramTester histogram_tester;

    LogInitiatePaymentResultAndLatency(result, base::Milliseconds(10));

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Pix.InitiatePayment.",
                      result ? "Success" : "Failure", ".Latency"}),
        /*sample=*/10,
        /*expected_count=*/1);
  }
}

TEST(FacilitatedPaymentsMetricsTest, LogInitiatePurchaseActionAttempt) {
  base::HistogramTester histogram_tester;

  LogInitiatePurchaseActionAttempt();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest,
     LogInitiatePurchaseActionResultAndLatency) {
  for (const std::string& result : {"Succeeded", "Failed", "Abandoned"}) {
    base::HistogramTester histogram_tester;

    LogInitiatePurchaseActionResultAndLatency(result, base::Milliseconds(10));

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Pix.InitiatePurchaseAction.", result,
                      ".Latency"}),
        /*sample=*/10,
        /*expected_count=*/1);
  }
}

TEST(FacilitatedPaymentsMetricsTest, LogFopSelectorShown) {
  base::HistogramTester histogram_tester;

  LogFopSelectorShown(true);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelector.Shown",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogTransactionResult_Success) {
  base::HistogramTester histogram_tester;

  LogTransactionResult(TransactionResult::kSuccess, TriggerSource::kDOMSearch,
                       base::Milliseconds(10),
                       ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.Transaction.Result",
      /*sample=*/TransactionResult::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.Transaction.Success.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogTransactionResult_Failed) {
  base::HistogramTester histogram_tester;

  LogTransactionResult(TransactionResult::kFailed, TriggerSource::kDOMSearch,
                       base::Milliseconds(10),
                       ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.Transaction.Result",
      /*sample=*/TransactionResult::kFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.Transaction.Failed.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogTransactionResult_Abandoned) {
  base::HistogramTester histogram_tester;

  LogTransactionResult(TransactionResult::kAbandoned, TriggerSource::kDOMSearch,
                       base::Milliseconds(10),
                       ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.Transaction.Result",
      /*sample=*/TransactionResult::kAbandoned,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.Transaction.Abandoned.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogPixFopSelectorShownLatency) {
  base::HistogramTester histogram_tester;

  LogPixFopSelectorShownLatency(base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelectorShown.LatencyAfterCopy",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

class FacilitatedPaymentsMetricsExitedReasonTest
    : public testing::TestWithParam<PayflowExitedReason> {};

TEST_P(FacilitatedPaymentsMetricsExitedReasonTest, LogPayflowExitedReason) {
  base::HistogramTester histogram_tester;

  LogPayflowExitedReason(GetParam());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/GetParam(),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsMetricsTest,
    FacilitatedPaymentsMetricsExitedReasonTest,
    testing::Values(PayflowExitedReason::kCodeValidatorFailed,
                    PayflowExitedReason::kInvalidCode,
                    PayflowExitedReason::kUserOptedOut,
                    PayflowExitedReason::kNoLinkedAccount,
                    PayflowExitedReason::kLandscapeScreenOrientation,
                    PayflowExitedReason::kApiClientNotAvailable,
                    PayflowExitedReason::kRiskDataNotAvailable,
                    PayflowExitedReason::kClientTokenNotAvailable,
                    PayflowExitedReason::kInitiatePaymentFailed,
                    PayflowExitedReason::kActionTokenNotAvailable,
                    PayflowExitedReason::kUserLoggedOut,
                    PayflowExitedReason::kFopSelectorClosedNotByUser,
                    PayflowExitedReason::kFopSelectorClosedByUser));

class FacilitatedPaymentsMetricsUkmTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogPixCodeCopied) {
  LogPixCodeCopied(ukm::UkmRecorder::GetNewSourceID());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeCopied::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeCopied::kPixCodeCopiedName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("PixCodeCopied"), true);
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogFopSelectorShown) {
  LogFopSelectorShownUkm(ukm::UkmRecorder::GetNewSourceID());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kEntryName,
      {ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kShownName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Shown"), true);
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogFopSelectorResult) {
  size_t index = 0;
  for (bool accepted : {true, false}) {
    LogFopSelectorResultUkm(accepted, ukm::UkmRecorder::GetNewSourceID());

    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), index + 1);
    EXPECT_EQ(ukm_entries[index++].metrics.at("Result"), accepted);
  }
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogInitiatePurchaseActionResult) {
  size_t index = 0;
  for (const std::string result : {"Succeeded", "Failed", "Abandoned"}) {
    LogInitiatePurchaseActionResultUkm(result,
                                       ukm::UkmRecorder::GetNewSourceID());

    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
            kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), index + 1);
    EXPECT_EQ(ukm_entries[index++].metrics.at("Result"),
              ConvertPurchaseActionResultToEnumValue(result));
  }
}

TEST_F(FacilitatedPaymentsMetricsUkmTest, LogTransactionResult_UkmLogged) {
  LogTransactionResult(TransactionResult::kSuccess, TriggerSource::kDOMSearch,
                       base::Milliseconds(10),
                       ukm::UkmRecorder::GetNewSourceID());
  task_environment_.RunUntilIdle();

  // Verify UKM histograms logged.
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Pix_Transaction::kEntryName,
      {ukm::builders::FacilitatedPayments_Pix_Transaction::kResultName,
       ukm::builders::FacilitatedPayments_Pix_Transaction::kTriggerSourceName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(TransactionResult::kSuccess));
  EXPECT_EQ(ukm_entries[0].metrics.at("TriggerSource"),
            static_cast<uint8_t>(TriggerSource::kDOMSearch));
}

class FacilitatedPaymentsMetricsTestForUiScreens
    : public testing::TestWithParam<UiState> {
 public:
  UiState ui_screen() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(FacilitatedPaymentsMetricsTest,
                         FacilitatedPaymentsMetricsTestForUiScreens,
                         testing::Values(UiState::kFopSelector,
                                         UiState::kProgressScreen,
                                         UiState::kErrorScreen));

TEST_P(FacilitatedPaymentsMetricsTestForUiScreens, LogUiScreenShown) {
  base::HistogramTester histogram_tester;

  LogUiScreenShown(ui_screen());

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.UiScreenShown",
                                      /*sample=*/ui_screen(),
                                      /*expected_bucket_count=*/1);
}

}  // namespace payments::facilitated
