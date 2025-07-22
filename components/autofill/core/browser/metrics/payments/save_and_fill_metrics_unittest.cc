// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class SaveAndFillMetricsTest : public AutofillMetricsBaseTest,
                               public testing::Test {
 public:
  SaveAndFillMetricsTest() = default;
  ~SaveAndFillMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

TEST_F(SaveAndFillMetricsTest, LogSuggestionShown) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillFormEvent(SaveAndFillFormEvent::kSuggestionShown);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.SaveAndFill",
      /*sample=*/SaveAndFillFormEvent::kSuggestionShown,
      /*expected_count=*/1);
}

TEST_F(SaveAndFillMetricsTest, LogSuggestionAccepted) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillFormEvent(SaveAndFillFormEvent::kSuggestionAccepted);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.SaveAndFill",
      /*sample=*/SaveAndFillFormEvent::kSuggestionAccepted,
      /*expected_count=*/1);
}

}  // namespace autofill::autofill_metrics
