// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;

UrlFilterSuggestion CreateSuggestionWithAttributes(
    std::string task_type,
    std::vector<std::pair<std::string, std::string>> attrs) {
  UrlFilterSuggestion::Params params;
  params.navigation_url = GURL("https://example.com");
  params.task_type = std::move(task_type);
  for (const auto& [key, val] : attrs) {
    params.attribute_ui_labels.emplace_back(
        FilterSuggestionCandidateAttribute(key, base::UTF8ToUTF16(key)),
        FilterAttribute(key, val));
  }
  return UrlFilterSuggestion(std::move(params));
}

UrlFilterSuggestion CreateSuggestion(std::string task_type) {
  return CreateSuggestionWithAttributes(std::move(task_type), {});
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

// Verifies that successful suggestion application logs kAllFiltersApplied.
TEST(MultistepFilterMetricsTrackerTest,
     SuggestionApplicationSuccessRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  FilterNavigationMetadata metadata;
  metadata.applied_suggestion = suggestion;

  MultistepFilterMetricsTracker tracker;
  tracker.OnNavigationFinished(metadata);
  tracker.OnSuggestionApplicationAnnotationExtractionFinished(
      /*was_applied_successfully=*/true);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kMultistepFilterApplicationOutcomeHistogram),
              BucketsAre(Bucket(
                  MultistepFilterApplicationOutcome::kAllFiltersApplied, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
      BucketsAre(
          Bucket(MultistepFilterApplicationOutcome::kAllFiltersApplied, 1)));
}

// Tests that failed suggestion application logs kNotAllFiltersApplied.
TEST(MultistepFilterMetricsTrackerTest,
     SuggestionApplicationFailureRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  FilterNavigationMetadata metadata;
  metadata.applied_suggestion = suggestion;

  MultistepFilterMetricsTracker tracker;
  tracker.OnNavigationFinished(metadata);
  tracker.OnSuggestionApplicationAnnotationExtractionFinished(
      /*was_applied_successfully=*/false);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(
          Bucket(MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
      BucketsAre(
          Bucket(MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
}

// Tests that calling annotation extraction finished without a session records
// no samples.
TEST(MultistepFilterMetricsTrackerTest,
     ApplicationFinishedWithoutSessionRecordsNoSamples) {
  base::HistogramTester histogram_tester;
  MultistepFilterMetricsTracker tracker;
  tracker.OnSuggestionApplicationAnnotationExtractionFinished(
      /*was_applied_successfully=*/true);

  histogram_tester.ExpectTotalCount(kMultistepFilterApplicationOutcomeHistogram,
                                    0);
}

// Tests that if a new navigation finishes while we were waiting for
// extraction of a previously applied suggestion, that application session
// is flushed as a failure.
TEST(MultistepFilterMetricsTrackerTest,
     ApplicationInterruptedByNewNavigationRecordsFailure) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  MultistepFilterMetricsTracker tracker;
  // 1. Landing navigation finishes (applying the suggestion).
  FilterNavigationMetadata landing_metadata;
  landing_metadata.applied_suggestion = suggestion;
  tracker.OnNavigationFinished(landing_metadata);

  // 3. Before extraction finishes, a new navigation finishes (e.g. user typed
  // new URL).
  FilterNavigationMetadata interrupt_metadata;
  tracker.OnNavigationFinished(interrupt_metadata);

  // The session should be immediately flushed as failure.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(
          Bucket(MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
      BucketsAre(
          Bucket(MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
}

// Tests that showing a suggestion logs the number of facets shown.
TEST(MultistepFilterMetricsTrackerTest, FacetsShownLogged) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS", {{"color", "blue"}, {"size", "large"}});
  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnSuggestionShown(suggestion);
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsShownHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsShown.ByTask.SEARCH_ACCOMMODATIONS", 2, 1);
}

// Tests that successful suggestion application logs the count of successfully
// applied facets and per-facet success.
TEST(MultistepFilterMetricsTrackerTest,
     SuggestionApplicationSuccessLogsFacetCountAndOutcomes) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS", {{"color", "blue"}, {"size", "large"}});
  FilterNavigationMetadata metadata;
  metadata.applied_suggestion = suggestion;

  MultistepFilterMetricsTracker tracker;
  tracker.OnNavigationFinished(metadata);
  tracker.OnSuggestionApplicationAnnotationExtractionFinished(
      /*was_applied_successfully=*/true);

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsSuccessfullyApplied.ByTask.SEARCH_"
      "ACCOMMODATIONS",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "color",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "size",
      true, 1);
}

// Tests that failed suggestion application logs per-facet failure and no
// success count.
TEST(MultistepFilterMetricsTrackerTest,
     SuggestionApplicationFailureLogsPerFacetOutcomes) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS", {{"color", "blue"}, {"size", "large"}});
  FilterNavigationMetadata metadata;
  metadata.applied_suggestion = suggestion;

  MultistepFilterMetricsTracker tracker;
  tracker.OnNavigationFinished(metadata);
  tracker.OnSuggestionApplicationAnnotationExtractionFinished(
      /*was_applied_successfully=*/false);

  histogram_tester.ExpectTotalCount(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 0);

  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "color",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "size",
      false, 1);
}

}  // namespace
}  // namespace multistep_filter
