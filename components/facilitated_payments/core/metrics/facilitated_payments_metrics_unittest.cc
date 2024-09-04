// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

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

TEST(FacilitatedPaymentsMetricsTest, LogIsAvailableResult) {
  base::HistogramTester histogram_tester;

  LogIsApiAvailableResult(/*result=*/true, base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Latency",
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

TEST(FacilitatedPaymentsMetricsTest, LogGetClientTokenResult) {
  base::HistogramTester histogram_tester;

  LogGetClientTokenResult(/*result=*/true, base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.GetClientToken.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.GetClientToken.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogPaymentNotOfferedReason) {
  base::HistogramTester histogram_tester;

  LogPaymentNotOfferedReason(PaymentNotOfferedReason::kApiNotAvailable);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason",
      /*sample=*/PaymentNotOfferedReason::kApiNotAvailable,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogInitiatePaymentResult) {
  base::HistogramTester histogram_tester;

  LogInitiatePaymentResult(/*result=*/true, base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
}

TEST(FacilitatedPaymentsMetricsTest, LogInitiatePurchaseActionResult) {
  base::HistogramTester histogram_tester;

  LogInitiatePurchaseActionResult(/*result=*/true, base::Milliseconds(10));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Latency",
      /*sample=*/10,
      /*expected_bucket_count=*/1);
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

class FacilitatedPaymentsMetricsUkmTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

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
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(TransactionResult::kSuccess));
  EXPECT_EQ(ukm_entries[0].metrics.at("TriggerSource"),
            static_cast<uint8_t>(TriggerSource::kDOMSearch));
}

}  // namespace payments::facilitated
