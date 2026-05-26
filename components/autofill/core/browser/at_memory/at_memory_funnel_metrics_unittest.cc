// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/aliases.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AtMemoryFunnelMetricsTest : public testing::Test {
 public:
  AtMemoryFunnelMetricsTest() = default;

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests that `OnPopupShown` correctly logs the "PopupDisplayed" metric when
// triggered by typing the invocation sequence.
TEST_F(AtMemoryFunnelMetricsTest, OnPopupShown_TypedTrigger) {
  AtMemoryFunnelMetrics metrics;
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.PopupDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that `OnPopupShown` correctly logs the "PopupDisplayed" metric when
// triggered via the context menu.
TEST_F(AtMemoryFunnelMetricsTest, OnPopupShown_ContextMenu) {
  AtMemoryFunnelMetrics metrics;
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.PopupDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kContextMenu, 1);
}

// Tests that `OnPopupShown` is idempotent and only logs a metric for the
// first call in a session.
TEST_F(AtMemoryFunnelMetricsTest, OnPopupShown_Idempotent) {
  AtMemoryFunnelMetrics metrics;
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
  // Second call should be ignored.
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.PopupDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that the destructor correctly logs that a query was submitted.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_QuerySubmitted_True) {
  {
    AtMemoryFunnelMetrics metrics;
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted();
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.QuerySubmitted", true, 1);
}

// Tests that the destructor correctly logs that no query was submitted
// during a shown session.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_QuerySubmitted_False) {
  {
    AtMemoryFunnelMetrics metrics;
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    // No query submitted.
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.QuerySubmitted", false, 1);
}

// Tests that the destructor correctly logs that a suggestion was accepted.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_SuggestionAccepted_True) {
  {
    AtMemoryFunnelMetrics metrics;
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted();
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", true, 1);
}

// Tests that the destructor correctly logs that no suggestion was accepted
// during a shown session.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_SuggestionAccepted_False) {
  {
    AtMemoryFunnelMetrics metrics;
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    // No suggestion accepted.
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", false, 1);
}

// Tests that `MarkFilled` correctly logs whether a suggestion was filled.
TEST_F(AtMemoryFunnelMetricsTest, MarkFilled_Filled) {
  {
    AtMemoryFunnelMetrics metrics;
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted();
    metrics.MarkFilled();
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionFilled", true, 1);

  {
    AtMemoryFunnelMetrics metrics2;
    metrics2.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics2.OnSuggestionAccepted();
  }

  histogram_tester_.ExpectBucketCount(
      "Autofill.AtMemory.Funnel.SuggestionFilled", false, 1);
}

}  // namespace autofill
