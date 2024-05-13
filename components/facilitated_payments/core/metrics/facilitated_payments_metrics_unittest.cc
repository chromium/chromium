// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

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

}  // namespace payments::facilitated
