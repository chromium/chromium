// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/filter_acceptance_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

// Verifies that destroying a logger when suggestion cue was not shown emits
// zero samples to any acceptance histograms.
TEST(FilterAcceptanceMetricsLoggerTest, NoSuggestionShownRecordsNoSamples) {
  base::HistogramTester histogram_tester;
  {
    FilterAcceptanceMetricsLogger logger("SEARCH_ACCOMMODATIONS");
  }
  histogram_tester.ExpectTotalCount(
      kMultistepFilterAcceptanceInitialCueHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterAcceptanceReopenedCueHistogram, 0);
  histogram_tester.ExpectTotalCount(kMultistepFilterAcceptanceHistogram, 0);
}

// Verifies that showing the initial cue without explicit user interaction
// defaults both initial cue and overall acceptance metrics to kIgnored upon
// destruction.
TEST(FilterAcceptanceMetricsLoggerTest,
     InitialCueShownAndIgnoredRecordsSample) {
  base::HistogramTester histogram_tester;
  {
    FilterAcceptanceMetricsLogger logger("SEARCH_ACCOMMODATIONS");
    logger.RecordInitialCueShown();
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.Acceptance.InitialCue.ByTask.SEARCH_ACCOMMODATIONS",
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.Acceptance.ByTask.SEARCH_ACCOMMODATIONS",
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterAcceptanceReopenedCueHistogram, 0);
}

// Verifies that interacting with the initial cue (e.g., dismissing it) records
// the explicit decision to both the initial cue and overall histograms.
TEST(FilterAcceptanceMetricsLoggerTest,
     InitialCueExplicitDecisionRecordsSample) {
  base::HistogramTester histogram_tester;
  {
    FilterAcceptanceMetricsLogger logger("SEARCH_ACCOMMODATIONS");
    logger.RecordInitialCueShown();
    logger.RecordInitialCueAndOverallDecision(
        SuggestionUserDecision::kDismissed);
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kDismissed, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kDismissed, 1);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterAcceptanceReopenedCueHistogram, 0);
}

// Verifies that displaying and accepting the reopened cue records kAccepted to
// the reopened cue and overall histograms without touching initial cue metrics.
TEST(FilterAcceptanceMetricsLoggerTest,
     ReopenedCueExplicitDecisionRecordsSample) {
  base::HistogramTester histogram_tester;
  {
    FilterAcceptanceMetricsLogger logger("SEARCH_FLIGHTS");
    logger.RecordReopenedCueShown();
    logger.RecordReopenedCueAndOverallDecision(
        SuggestionUserDecision::kAccepted);
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceReopenedCueHistogram,
      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.Acceptance.ReopenedCue.ByTask.SEARCH_FLIGHTS",
      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterAcceptanceInitialCueHistogram, 0);
}

// Verifies that opening multiple surfaces and toggling the reopened cue
// multiple times deduplicates samples and records exactly one UMA entry per
// engaged surface upon destruction.
TEST(FilterAcceptanceMetricsLoggerTest,
     ReopenedCueMultipleTimesRecordsSingleSample) {
  base::HistogramTester histogram_tester;
  {
    FilterAcceptanceMetricsLogger logger("SEARCH_FLIGHTS");
    logger.RecordInitialCueShown();
    // Default ignored on initial cue
    logger.RecordReopenedCueShown();
    // Reopened cue multiple times
    logger.RecordReopenedCueShown();
    logger.RecordReopenedCueShown();
    logger.RecordReopenedCueAndOverallDecision(
        SuggestionUserDecision::kDismissed);
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceReopenedCueHistogram,
      SuggestionUserDecision::kDismissed, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kDismissed, 1);
}

// Verifies that showing both initial and reopened cues without invoking
// any decision methods defaults all surface and overall metrics to kIgnored
// upon destruction.
TEST(FilterAcceptanceMetricsLoggerTest,
     BothCueSurfacesShownWithoutActionRecordsAllIgnored) {
  base::HistogramTester histogram_tester;
  {
    FilterAcceptanceMetricsLogger logger("SEARCH_ACCOMMODATIONS");
    logger.RecordInitialCueShown();
    logger.RecordReopenedCueShown();
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceReopenedCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
}

}  // namespace
}  // namespace multistep_filter
