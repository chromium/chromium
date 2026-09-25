// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/promo_code_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

TEST(PromoCodeMetricsTest, LogPromoCodeFormEvent) {
  base::HistogramTester histogram_tester;

  LogPromoCodeFormEvent(PromoCodeFormEvent::kPromoCodeSuggestionsShown);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      PromoCodeFormEvent::kPromoCodeSuggestionsShown, 1);
}

}  // namespace
}  // namespace autofill::autofill_metrics
