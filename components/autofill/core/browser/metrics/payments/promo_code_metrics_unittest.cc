// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/promo_code_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill::autofill_metrics {
namespace {

TEST(PromoCodeMetricsTest, LogPromoCodeFormEvent) {
  base::HistogramTester histogram_tester;

  LogPromoCodeFormEvent(PromoCodeFormEvent::kPromoCodeSuggestionsShown);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      PromoCodeFormEvent::kPromoCodeSuggestionsShown, 1);

  LogPromoCodeFormEvent(PromoCodeFormEvent::kPromoCodeSuggestionFilled);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      PromoCodeFormEvent::kPromoCodeSuggestionFilled, 1);
}

// Tests that the total, valid and expired stored promo code counts are logged.
TEST(PromoCodeMetricsTest, LogStoredPromoCodeMetrics) {
  base::HistogramTester histogram_tester;
  const AutofillOfferData valid_promo_code_1 = test::GetPromoCodeOfferData(
      GURL("https://www.example.com"), /*is_expired=*/false,
      /*offer_id=*/"111");
  const AutofillOfferData valid_promo_code_2 = test::GetPromoCodeOfferData(
      GURL("https://www.example.com"), /*is_expired=*/false,
      /*offer_id=*/"222");
  const AutofillOfferData expired_promo_code = test::GetPromoCodeOfferData(
      GURL("https://www.example.com"), /*is_expired=*/true,
      /*offer_id=*/"333");

  LogStoredPromoCodeMetrics(
      {&valid_promo_code_1, &valid_promo_code_2, &expired_promo_code});

  histogram_tester.ExpectUniqueSample("Autofill.StoredPromoCodeCount", 3, 1);
  histogram_tester.ExpectUniqueSample("Autofill.StoredPromoCodeCount.ValidCode",
                                      2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredPromoCodeCount.ExpiredCode", 1, 1);
}

// Tests that counts of zero are logged when no promo codes are stored.
TEST(PromoCodeMetricsTest, LogStoredPromoCodeMetrics_NoPromoCodes) {
  base::HistogramTester histogram_tester;

  LogStoredPromoCodeMetrics({});

  histogram_tester.ExpectUniqueSample("Autofill.StoredPromoCodeCount", 0, 1);
  histogram_tester.ExpectUniqueSample("Autofill.StoredPromoCodeCount.ValidCode",
                                      0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredPromoCodeCount.ExpiredCode", 0, 1);
}

}  // namespace
}  // namespace autofill::autofill_metrics
