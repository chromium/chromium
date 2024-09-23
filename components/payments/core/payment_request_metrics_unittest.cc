// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

TEST(PaymentRequestMetricsTest, RecordCanMakePaymentPrefMetrics_Enabled) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(kCanMakePaymentEnabled, true);

  base::HistogramTester histogram_tester;
  RecordCanMakePaymentPrefMetrics(pref_service, "Suffix");
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix", /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix.DisabledReason",
      /*expected_count=*/0);
}

TEST(PaymentRequestMetricsTest,
     RecordCanMakePaymentPrefMetrics_DisabledByUser) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(kCanMakePaymentEnabled, true);
  pref_service.SetUserPref(kCanMakePaymentEnabled, base::Value(false));

  base::HistogramTester histogram_tester;
  RecordCanMakePaymentPrefMetrics(pref_service, "Suffix");
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix", /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix.DisabledReason",
      CanMakePaymentPreferenceSetter::kUserSetting,
      /*expected_bucket_count=*/1);
}

TEST(PaymentRequestMetricsTest,
     RecordCanMakePaymentPrefMetrics_DisabledByExtension) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(kCanMakePaymentEnabled, true);
  pref_service.SetExtensionPref(kCanMakePaymentEnabled, base::Value(false));

  base::HistogramTester histogram_tester;
  RecordCanMakePaymentPrefMetrics(pref_service, "Suffix");
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix", /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix.DisabledReason",
      CanMakePaymentPreferenceSetter::kExtension, /*expected_bucket_count=*/1);
}

TEST(PaymentRequestMetricsTest,
     RecordCanMakePaymentPrefMetrics_DisabledByCustodian) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(kCanMakePaymentEnabled, true);
  pref_service.SetSupervisedUserPref(kCanMakePaymentEnabled,
                                     base::Value(false));

  base::HistogramTester histogram_tester;
  RecordCanMakePaymentPrefMetrics(pref_service, "Suffix");
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix", /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix.DisabledReason",
      CanMakePaymentPreferenceSetter::kCustodian, /*expected_bucket_count=*/1);
}

TEST(PaymentRequestMetricsTest,
     RecordCanMakePaymentPrefMetrics_DisabledByManaged) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(kCanMakePaymentEnabled, true);
  pref_service.SetManagedPref(kCanMakePaymentEnabled, base::Value(false));

  base::HistogramTester histogram_tester;
  RecordCanMakePaymentPrefMetrics(pref_service, "Suffix");
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix", /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix.DisabledReason",
      CanMakePaymentPreferenceSetter::kAdminPolicy,
      /*expected_bucket_count=*/1);
}

TEST(PaymentRequestMetricsTest,
     RecordCanMakePaymentPrefMetrics_DisabledByRecommended) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(kCanMakePaymentEnabled, true);
  pref_service.SetRecommendedPref(kCanMakePaymentEnabled, base::Value(false));

  base::HistogramTester histogram_tester;
  RecordCanMakePaymentPrefMetrics(pref_service, "Suffix");
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix", /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.IsCanMakePaymentAllowedByPref.Suffix.DisabledReason",
      CanMakePaymentPreferenceSetter::kAdminPolicy,
      /*expected_bucket_count=*/1);
}

}  // namespace payments
