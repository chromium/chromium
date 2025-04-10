// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// TODO(crbug.com/409565194): Move `Autofill.FormEvents.CreditCard` tests from
// `autofill_metrics_unittest.cc` to
// `credit_card_form_event_logger_unittest.cc`.
class CreditCardFormEventLoggerTest : public AutofillMetricsBaseTest,
                                      public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

// Tests that the `kBnplSuggestionAcceptedOnce` event is logged once when
// `OnDidAcceptBnplSuggestion()` is called.
TEST_F(CreditCardFormEventLoggerTest,
       OnDidAcceptBnplSuggestion_SuggestionAcceptedLogged) {
  base::HistogramTester histogram_tester;

  autofill_manager().GetCreditCardFormEventLogger().OnDidAcceptBnplSuggestion();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAcceptedOnce,
      /*expected_bucket_count=*/1);

  // Test that `kBnplSuggestionAcceptedOnce` is logged only once even if
  // `OnDidAcceptBnplSuggestion()` is called more than once on the same page.
  autofill_manager().GetCreditCardFormEventLogger().OnDidAcceptBnplSuggestion();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAcceptedOnce,
      /*expected_bucket_count=*/1);
}

}  // namespace autofill::autofill_metrics
