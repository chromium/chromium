// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

UrlFilterSuggestion CreateSuggestion(std::string task_type) {
  UrlFilterSuggestion::Params params;
  params.navigation_url = GURL("https://example.com");
  params.task_type = std::move(task_type);
  return UrlFilterSuggestion(std::move(params));
}

// Verifies that destroying a tracker when no suggestion was shown records
// no samples.
TEST(MultistepFilterMetricsTrackerTest, NoSuggestionShownRecordsNoSamples) {
  base::HistogramTester histogram_tester;
  {
    MultistepFilterMetricsTracker tracker;
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
TEST(MultistepFilterMetricsTrackerTest,
     InitialCueShownAndIgnoredRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnSuggestionShown(suggestion);
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
TEST(MultistepFilterMetricsTrackerTest,
     InitialCueExplicitDecisionRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnSuggestionShown(suggestion);
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kDismissed);
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
// the reopened cue and overall histograms, and records kIgnored to initial cue.
TEST(MultistepFilterMetricsTrackerTest,
     ReopenedCueExplicitDecisionRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_FLIGHTS");
  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnSuggestionShown(suggestion);
    tracker.OnSuggestionReopened();
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceReopenedCueHistogram,
      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.Acceptance.ReopenedCue.ByTask.SEARCH_FLIGHTS",
      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kAccepted, 1);
}

// Verifies that opening multiple surfaces and toggling the reopened cue
// multiple times deduplicates samples and records exactly one UMA entry per
// engaged surface.
TEST(MultistepFilterMetricsTrackerTest,
     ReopenedCueMultipleTimesRecordsSingleSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_FLIGHTS");
  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnSuggestionShown(suggestion);
    tracker.OnSuggestionReopened();
    tracker.OnSuggestionReopened();
    tracker.OnSuggestionReopened();
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kDismissed);
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
// any decision methods defaults all surfaces and overall metrics to kIgnored
// upon destruction.
TEST(MultistepFilterMetricsTrackerTest,
     BothCueSurfacesShownWithoutActionRecordsAllIgnored) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnSuggestionShown(suggestion);
    tracker.OnSuggestionReopened();
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
