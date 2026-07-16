// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "components/multistep_filter/core/prefs/retention_state_snapshot.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;

class MultistepFilterMetricsTrackerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

FilterNavigationMetadata CreateDefaultMetadata() {
  FilterNavigationMetadata metadata;
  base::TimeTicks now = base::TimeTicks::Now();
  metadata.navigation_start_time = now;
  metadata.navigation_finish_time = now;
  return metadata;
}

void TriggerInitialNavigation(MultistepFilterMetricsTracker& tracker) {
  tracker.OnNavigationFinished(CreateDefaultMetadata());
}

void SetupAcceptedAndLandedSession(MultistepFilterMetricsTracker& tracker,
                                   const UrlFilterSuggestion& suggestion) {
  TriggerInitialNavigation(tracker);
  tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
  tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);

  FilterNavigationMetadata landing_metadata = CreateDefaultMetadata();
  landing_metadata.url = GURL("https://example.com/landing");
  landing_metadata.prev_url = GURL("https://example.com/source");
  landing_metadata.applied_suggestion = suggestion;
  tracker.OnNavigationFinished(landing_metadata);
}

void SetupPostSuggestionApplicationSession(
    MultistepFilterMetricsTracker& tracker,
    const UrlFilterSuggestion& suggestion) {
  SetupAcceptedAndLandedSession(tracker, suggestion);
  tracker.OnSuggestionApplicationAnnotationExtractionFinished(
      /*was_applied_successfully=*/true);
}

UrlFilterSuggestion CreateSuggestionWithAttributes(
    std::string task_type,
    std::vector<std::pair<std::string, std::string>> attrs) {
  UrlFilterSuggestion::Params params;
  params.navigation_url = GURL("https://example.com");
  params.source_host = u"source.com";
  params.triggering_host = "trigger.com";
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

void VerifyAcceptanceRetentionHistograms(
    const base::HistogramTester& histogram_tester,
    base::span<const std::string_view> expected_slices,
    SuggestionUserDecision expected_decision,
    int expected_count = 1) {
  for (const auto& slice : kAllRetentionSlices) {
    std::string initial_name = base::StrCat(
        {"MultistepFilter.Acceptance.InitialCue.ByRetention.", slice});
    std::string overall_name =
        base::StrCat({"MultistepFilter.Acceptance.ByRetention.", slice});
    if (std::ranges::contains(expected_slices, slice)) {
      histogram_tester.ExpectUniqueSample(initial_name, expected_decision,
                                          expected_count);
      histogram_tester.ExpectUniqueSample(overall_name, expected_decision,
                                          expected_count);
    } else {
      histogram_tester.ExpectTotalCount(initial_name, 0);
      histogram_tester.ExpectTotalCount(overall_name, 0);
    }
  }
}

void VerifyApplicationOutcomeRetentionHistograms(
    const base::HistogramTester& histogram_tester,
    base::span<const std::string_view> expected_slices,
    MultistepFilterApplicationOutcome expected_outcome,
    int expected_count = 1) {
  for (const auto& slice : kAllRetentionSlices) {
    std::string name = base::StrCat(
        {"MultistepFilter.ApplicationOutcome.ByRetention.", slice});
    if (std::ranges::contains(expected_slices, slice)) {
      histogram_tester.ExpectUniqueSample(name, expected_outcome,
                                          expected_count);
    } else {
      histogram_tester.ExpectTotalCount(name, 0);
    }
  }
}

// Verifies that destroying a tracker when no suggestion was shown records
// no samples.
TEST_F(MultistepFilterMetricsTrackerTest, NoSuggestionShownRecordsNoSamples) {
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
TEST_F(MultistepFilterMetricsTrackerTest,
       InitialCueShownAndIgnoredRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  suggestion.extraction_timestamp = base::Time::Now();  // T0
  task_environment_.FastForwardBy(base::Minutes(10) -
                                  base::Milliseconds(300));  // T0 + 9m 59.7s
  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    task_environment_.FastForwardBy(base::Milliseconds(300));  // T0 + 10m
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
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
  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeNavigationToSuggestionShownHistogram,
      base::Milliseconds(300), 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterSuggestionAgeShownHistogram, 10, 1);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterSuggestionAgeShownOnSameDomainHistogram, 0);
}

// Verifies that showing the initial cue without explicit user interaction
// records kIgnored to both initial cue and overall histograms upon navigation.
TEST_F(MultistepFilterMetricsTrackerTest,
       InitialCueIgnoredOnNavigationRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  MultistepFilterMetricsTracker tracker;
  TriggerInitialNavigation(tracker);

  // Show suggestion.
  tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());

  // Trigger navigation to a different page.
  FilterNavigationMetadata metadata = CreateDefaultMetadata();
  metadata.url = GURL("https://different.com");
  metadata.prev_url = GURL("https://example.com");
  metadata.is_same_document_navigation = false;

  tracker.OnNavigationFinished(metadata);

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
}

// Verifies that interacting with the initial cue (e.g., dismissing it) records
// the explicit decision to both the initial cue and overall histograms.
TEST_F(MultistepFilterMetricsTrackerTest,
       InitialCueExplicitDecisionRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
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
TEST_F(MultistepFilterMetricsTrackerTest,
       ReopenedCueExplicitDecisionRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_FLIGHTS");
  suggestion.extraction_timestamp = base::Time::Now();  // T0
  task_environment_.FastForwardBy(base::Minutes(10) -
                                  base::Milliseconds(300));  // T0 + 9m 59.7s
  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    task_environment_.FastForwardBy(base::Milliseconds(300));  // T0 + 10m
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
    tracker.OnSuggestionReopened();
    task_environment_.FastForwardBy(base::Minutes(5));  // T0 + 15m
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
  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeNavigationToSuggestionAcceptedHistogram,
      base::Minutes(5) + base::Milliseconds(300), 1);
  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeSuggestionShownToAcceptedHistogram, base::Minutes(5),
      1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterSuggestionAgeShownHistogram, 10, 1);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterSuggestionAgeShownOnSameDomainHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterSuggestionAgeAcceptedHistogram, 15, 1);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterSuggestionAgeAcceptedOnSameDomainHistogram, 0);
}

// Verifies that the suggestion age is logged to the same domain histogram when
// the reopened cue is accepted on the same domain.
TEST_F(MultistepFilterMetricsTrackerTest,
       ReopenedCueAcceptedOnSameDomainAgeLogged) {
  base::HistogramTester histogram_tester;

  // Suggestion created with matching hosts.
  UrlFilterSuggestion::Params params;
  params.navigation_url = GURL("https://booking.com/landing");
  params.source_host = u"flights.booking.com";
  params.triggering_host = "booking.com";
  params.extraction_timestamp = base::Time::Now();  // T0
  params.task_type = "SEARCH_FLIGHTS";
  UrlFilterSuggestion suggestion(std::move(params));
  task_environment_.FastForwardBy(base::Minutes(10) -
                                  base::Milliseconds(300));  // T0 + 9m 59.7s
  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    task_environment_.FastForwardBy(base::Milliseconds(300));  // T0 + 10m
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
    tracker.OnSuggestionReopened();
    task_environment_.FastForwardBy(base::Minutes(5));  // T0 + 15m
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterSuggestionAgeShownHistogram, 10, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterSuggestionAgeShownOnSameDomainHistogram, 10, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterSuggestionAgeAcceptedHistogram, 15, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterSuggestionAgeAcceptedOnSameDomainHistogram, 15, 1);
}

// Verifies that opening multiple surfaces and toggling the reopened cue
// multiple times deduplicates samples and records exactly one UMA entry per
// engaged surface.
TEST_F(MultistepFilterMetricsTrackerTest,
       ReopenedCueMultipleTimesRecordsSingleSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_FLIGHTS");
  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
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
TEST_F(MultistepFilterMetricsTrackerTest,
       BothCueSurfacesShownWithoutActionRecordsAllIgnored) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
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
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionApplicationSuccessRecordsSample) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  FilterNavigationMetadata metadata;
  metadata.applied_suggestion = suggestion;
  base::TimeTicks accepted_time = base::TimeTicks::Now();
  metadata.navigation_start_time = accepted_time;
  MultistepFilterMetricsTracker tracker;
  task_environment_.FastForwardBy(base::Milliseconds(2000));
  metadata.navigation_finish_time = base::TimeTicks::Now();
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
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      MultistepFilterApplicationOutcome::kAllFiltersApplied);
  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram,
      base::Milliseconds(2000), 1);
}

// Tests that failed suggestion application logs kNotAllFiltersApplied and
// records no latency.
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionApplicationFailureRecordsSample) {
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  // Scenario 1: Extraction fails (normal navigation).
  {
    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.applied_suggestion = suggestion;
    tracker.OnNavigationFinished(metadata);
    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/false);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            kMultistepFilterApplicationOutcomeHistogram),
        BucketsAre(Bucket(
            MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
        BucketsAre(Bucket(
            MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester, {kRetentionSliceFirstImpression},
        MultistepFilterApplicationOutcome::kNotAllFiltersApplied);
    histogram_tester.ExpectTotalCount(
        kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram, 0);
  }

  // Scenario 2: Navigation error page (even if extraction succeeds).
  {
    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.applied_suggestion = suggestion;
    metadata.is_error_page_navigation = true;
    tracker.OnNavigationFinished(metadata);
    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/true);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            kMultistepFilterApplicationOutcomeHistogram),
        BucketsAre(Bucket(
            MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
    histogram_tester.ExpectTotalCount(
        kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram, 0);
  }
}

// Tests that calling annotation extraction finished without a session records
// no samples.
TEST_F(MultistepFilterMetricsTrackerTest,
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
TEST_F(MultistepFilterMetricsTrackerTest,
       ApplicationInterruptedByNewNavigationRecordsFailure) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  MultistepFilterMetricsTracker tracker;
  // 1. Landing navigation finishes (applying the suggestion).
  FilterNavigationMetadata landing_metadata = CreateDefaultMetadata();
  landing_metadata.applied_suggestion = suggestion;
  tracker.OnNavigationFinished(landing_metadata);

  // 3. Before extraction finishes, a new navigation finishes (e.g. user typed
  // new URL).
  FilterNavigationMetadata interrupt_metadata = CreateDefaultMetadata();
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
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      MultistepFilterApplicationOutcome::kNotAllFiltersApplied);
}

// Tests that showing a suggestion logs the number of facets shown.
TEST_F(MultistepFilterMetricsTrackerTest, FacetsShownLogged) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS", {{"color", "blue"}, {"size", "large"}});
  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsShownHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsShown.ByTask.SEARCH_ACCOMMODATIONS", 2, 1);
}

// Tests that successful suggestion application logs the count of successfully
// applied facets and per-facet success.
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionApplicationSuccessLogsFacetCountAndOutcomes) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS", {{"color", "blue"}, {"size", "large"}});
  FilterNavigationMetadata metadata = CreateDefaultMetadata();
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
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionApplicationFailureLogsPerFacetOutcomes) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS", {{"color", "blue"}, {"size", "large"}});
  FilterNavigationMetadata metadata = CreateDefaultMetadata();
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

TEST_F(MultistepFilterMetricsTrackerTest, RetentionSlicedAcceptanceLogged) {
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  // Scenario 1: First Impression.
  {
    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);

    VerifyAcceptanceRetentionHistograms(histogram_tester,
                                        {kRetentionSliceFirstImpression},
                                        SuggestionUserDecision::kAccepted);
  }

  // Scenario 2: AcceptedLastTime + AcceptedAtLeastOnce.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 1;
    snapshot.suggestion_acceptances = 1;
    snapshot.is_last_suggestion_accepted = true;

    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, snapshot);
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kDismissed);

    VerifyAcceptanceRetentionHistograms(
        histogram_tester,
        {kRetentionSliceAcceptedLastTime, kRetentionSliceAcceptedAtLeastOnce},
        SuggestionUserDecision::kDismissed);
  }

  // Scenario 3: RejectedLastTime + SawCuesButNeverAccepted.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 2;
    snapshot.suggestion_acceptances = 0;
    snapshot.is_last_suggestion_accepted = false;

    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, snapshot);
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kDismissed);

    VerifyAcceptanceRetentionHistograms(
        histogram_tester,
        {kRetentionSliceRejectedLastTime,
         kRetentionSliceSawCuesButNeverAccepted},
        SuggestionUserDecision::kDismissed);
  }

  // Scenario 4: RejectedLastTime + AcceptedAtLeastOnce.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 2;
    snapshot.suggestion_acceptances = 1;
    snapshot.is_last_suggestion_accepted = false;

    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, snapshot);
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kDismissed);

    VerifyAcceptanceRetentionHistograms(
        histogram_tester,
        {kRetentionSliceRejectedLastTime, kRetentionSliceAcceptedAtLeastOnce},
        SuggestionUserDecision::kDismissed);
  }
}

TEST_F(MultistepFilterMetricsTrackerTest,
       RetentionSlicedApplicationOutcomeLogged) {
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  FilterNavigationMetadata landing_metadata = CreateDefaultMetadata();
  landing_metadata.applied_suggestion = suggestion;

  // Scenario 1: First Impression.
  {
    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
    tracker.OnNavigationFinished(landing_metadata);
    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/true);

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester, {kRetentionSliceFirstImpression},
        MultistepFilterApplicationOutcome::kAllFiltersApplied);
  }

  // Scenario 2: AcceptedLastTime + AcceptedAtLeastOnce.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 1;
    snapshot.suggestion_acceptances = 1;
    snapshot.is_last_suggestion_accepted = true;

    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, snapshot);
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
    tracker.OnNavigationFinished(landing_metadata);
    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/true);

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester,
        {kRetentionSliceAcceptedLastTime, kRetentionSliceAcceptedAtLeastOnce},
        MultistepFilterApplicationOutcome::kAllFiltersApplied);
  }

  // Scenario 3: RejectedLastTime + SawCuesButNeverAccepted.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 2;
    snapshot.suggestion_acceptances = 0;
    snapshot.is_last_suggestion_accepted = false;

    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, snapshot);
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
    tracker.OnNavigationFinished(landing_metadata);
    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/false);

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester,
        {kRetentionSliceRejectedLastTime,
         kRetentionSliceSawCuesButNeverAccepted},
        MultistepFilterApplicationOutcome::kNotAllFiltersApplied);
  }

  // Scenario 4: RejectedLastTime + AcceptedAtLeastOnce.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 2;
    snapshot.suggestion_acceptances = 1;
    snapshot.is_last_suggestion_accepted = false;

    base::HistogramTester histogram_tester;
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, snapshot);
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
    tracker.OnNavigationFinished(landing_metadata);
    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/true);

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester,
        {kRetentionSliceRejectedLastTime, kRetentionSliceAcceptedAtLeastOnce},
        MultistepFilterApplicationOutcome::kAllFiltersApplied);
  }
}

TEST_F(MultistepFilterMetricsTrackerTest,
       ApplicationRetentionSnapshotResetOnInterveningNavigation) {
  base::HistogramTester histogram_tester;
  MultistepFilterMetricsTracker tracker;

  UrlFilterSuggestion suggestion = CreateSuggestion("task_type");

  // 1. Suggestion shown with a non-default snapshot (AcceptedLastTime).
  RetentionStateSnapshot snapshot;
  snapshot.suggestion_impressions = 2;
  snapshot.suggestion_acceptances = 1;
  snapshot.is_last_suggestion_accepted = true;
  TriggerInitialNavigation(tracker);
  tracker.OnSuggestionShown(suggestion, snapshot);

  // 2. User accepts the suggestion. This caches the snapshot.
  tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);

  // 3. An intervening normal navigation occurs (not applying suggestion).
  // This must reset the cached snapshot.
  FilterNavigationMetadata normal_metadata = CreateDefaultMetadata();
  normal_metadata.url = GURL("https://example.com/normal");
  tracker.OnNavigationFinished(normal_metadata);

  // 4. Later, a suggestion is applied.
  FilterNavigationMetadata landing_metadata = CreateDefaultMetadata();
  landing_metadata.url = GURL("https://example.com/landing");
  landing_metadata.applied_suggestion = suggestion;
  tracker.OnNavigationFinished(landing_metadata);

  // 5. Extraction finishes.
  tracker.OnSuggestionApplicationAnnotationExtractionFinished(
      /*was_applied_successfully=*/true);

  // 6. Verify that the outcome is logged under 'FirstImpression' (the fallback
  // default snapshot) and NOT 'AcceptedLastTime'.
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      MultistepFilterApplicationOutcome::kAllFiltersApplied);
}

// Tests that showing a suggestion while another is already cached flushes the
// previous suggestion as ignored.
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionReplacementFlushesPreviousAsIgnored) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion1 = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  UrlFilterSuggestion suggestion2 = CreateSuggestion("SEARCH_FLIGHTS");

  MultistepFilterMetricsTracker tracker;
  TriggerInitialNavigation(tracker);
  tracker.OnSuggestionShown(suggestion1, RetentionStateSnapshot());
  histogram_tester.ExpectTotalCount(kMultistepFilterAcceptanceHistogram, 0);
  tracker.OnSuggestionShown(suggestion2, RetentionStateSnapshot());
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
}

// Tests that preserving a suggestion cleared flushes the active session as
// ignored.
TEST_F(MultistepFilterMetricsTrackerTest,
       PreservedSuggestionClearedFlushesActiveSession) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  MultistepFilterMetricsTracker tracker;
  FilterNavigationMetadata metadata = CreateDefaultMetadata();
  metadata.url = GURL("https://example.com");
  tracker.OnNavigationFinished(metadata);
  tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
  FilterNavigationMetadata same_page_metadata = CreateDefaultMetadata();
  same_page_metadata.url = GURL("https://example.com/#hash");
  same_page_metadata.prev_url = GURL("https://example.com");
  same_page_metadata.is_same_document_navigation = true;
  tracker.OnNavigationFinished(same_page_metadata);
  tracker.OnPreservedSuggestionCleared();
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
}

// Tests that same page navigations do not flush the active UI session.
TEST_F(MultistepFilterMetricsTrackerTest,
       SamePageNavigationsDoNotFlushUiSession) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());

    FilterNavigationMetadata same_doc_metadata = CreateDefaultMetadata();
    same_doc_metadata.url = GURL("https://example.com/landing#hash");
    same_doc_metadata.prev_url = GURL("https://example.com/landing");
    same_doc_metadata.is_same_document_navigation = true;
    tracker.OnNavigationFinished(same_doc_metadata);

    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
  }

  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());

    FilterNavigationMetadata reload_metadata = CreateDefaultMetadata();
    reload_metadata.url = GURL("https://example.com");
    reload_metadata.prev_url = GURL("https://example.com");
    tracker.OnNavigationFinished(reload_metadata);

    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
  }

  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kAccepted, 2);
}

// Tests that back navigations after the session window are logged correctly.
TEST_F(MultistepFilterMetricsTrackerTest, PostAcceptanceNavigations) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Minutes(3));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://example.com/source");
    metadata.prev_url = GURL("https://example.com/landing");
    metadata.is_back_navigation = true;
    metadata.navigation_start_time =
        task_environment_.NowTicks() - base::Minutes(2) - base::Seconds(50);
    tracker.OnNavigationFinished(metadata);
  }

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Minutes(3));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://example.com/source");
    metadata.prev_url = GURL("https://example.com/landing");
    metadata.is_back_navigation = true;
    tracker.OnNavigationFinished(metadata);
  }

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://example.com/other");
    metadata.prev_url = GURL("https://example.com/landing");
    tracker.OnNavigationFinished(metadata);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationFirstNavigationHistogram),
      BucketsAre(
          Bucket(MultistepFilterPostSuggestionApplicationFirstNavigation::
                     kBackNavigationWithinSessionWindow,
                 1),
          Bucket(MultistepFilterPostSuggestionApplicationFirstNavigation::
                     kBackNavigationAfterSessionWindow,
                 1),
          Bucket(MultistepFilterPostSuggestionApplicationFirstNavigation::
                     kForwardOrOtherNavigation,
                 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationTabCloseHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationTabClose::
                            kTabClosedWithFurtherNavigation,
                        3)));
}

// Tests that tab closure is logged correctly.
TEST_F(MultistepFilterMetricsTrackerTest, PostAcceptanceTabCloses) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Seconds(10));
  }

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Minutes(3));
  }

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://example.com/other");
    metadata.prev_url = GURL("https://example.com/landing");
    tracker.OnNavigationFinished(metadata);
    task_environment_.FastForwardBy(base::Seconds(10));
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationTabCloseHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationTabClose::
                            kTabClosedWithinSessionWindow,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationTabClose::
                            kTabClosedAfterSessionWindow,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationTabClose::
                            kTabClosedWithFurtherNavigation,
                        1)));
}

// Tests that ignored navigations and failures do not flush the active
// post-suggestion application session.
TEST_F(MultistepFilterMetricsTrackerTest,
       PostAcceptanceIgnoredNavigationsAndFailures) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);

    FilterNavigationMetadata same_doc_metadata = CreateDefaultMetadata();
    same_doc_metadata.url = GURL("https://example.com/landing#hash");
    same_doc_metadata.prev_url = GURL("https://example.com/landing");
    same_doc_metadata.is_same_document_navigation = true;
    tracker.OnNavigationFinished(same_doc_metadata);

    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata real_metadata = CreateDefaultMetadata();
    real_metadata.url = GURL("https://example.com/source");
    real_metadata.prev_url = GURL("https://example.com/landing#hash");
    real_metadata.is_back_navigation = true;
    tracker.OnNavigationFinished(real_metadata);
  }

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);

    FilterNavigationMetadata reload_metadata = CreateDefaultMetadata();
    reload_metadata.url = GURL("https://example.com/landing");
    reload_metadata.prev_url = GURL("https://example.com/landing");
    tracker.OnNavigationFinished(reload_metadata);
  }

  {
    MultistepFilterMetricsTracker tracker;
    SetupAcceptedAndLandedSession(tracker, suggestion);
    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/false);

    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata back_metadata = CreateDefaultMetadata();
    back_metadata.url = GURL("https://example.com/source");
    back_metadata.prev_url = GURL("https://example.com/landing");
    back_metadata.is_back_navigation = true;
    tracker.OnNavigationFinished(back_metadata);
  }

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterPostSuggestionApplicationFirstNavigationHistogram,
      MultistepFilterPostSuggestionApplicationFirstNavigation::
          kBackNavigationWithinSessionWindow,
      1);
}

// Tests that showing a suggestion while a post-application session is active
// does not interfere with the post-application session.
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionClearedDoesNotInterfereWithPostApplicationSession) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion1 = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  UrlFilterSuggestion suggestion2 = CreateSuggestion("SEARCH_FLIGHTS");

  MultistepFilterMetricsTracker tracker;
  SetupPostSuggestionApplicationSession(tracker, suggestion1);

  tracker.OnSuggestionShown(suggestion2, RetentionStateSnapshot());
  tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kIgnored);
  task_environment_.FastForwardBy(base::Seconds(10));
  FilterNavigationMetadata metadata = CreateDefaultMetadata();
  metadata.url = GURL("https://example.com/source");
  metadata.prev_url = GURL("https://example.com/landing");
  metadata.is_back_navigation = true;
  tracker.OnNavigationFinished(metadata);

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterPostSuggestionApplicationFirstNavigationHistogram,
      MultistepFilterPostSuggestionApplicationFirstNavigation::
          kBackNavigationWithinSessionWindow,
      1);
}

// Tests that a new suggestion application flushes the active
// post-suggestion application session.
TEST_F(MultistepFilterMetricsTrackerTest,
       NewSuggestionApplicationFlushesActivePostSession) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion1 = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  UrlFilterSuggestion suggestion2 = CreateSuggestion("SEARCH_FLIGHTS");

  {
    MultistepFilterMetricsTracker tracker;

    SetupPostSuggestionApplicationSession(tracker, suggestion1);

    tracker.OnSuggestionShown(suggestion2, RetentionStateSnapshot());
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);

    FilterNavigationMetadata landing_metadata2 = CreateDefaultMetadata();
    landing_metadata2.url = GURL("https://example.com/landing2");
    landing_metadata2.prev_url = GURL("https://example.com/landing");
    landing_metadata2.applied_suggestion = suggestion2;
    tracker.OnNavigationFinished(landing_metadata2);

    histogram_tester.ExpectUniqueSample(
        kMultistepFilterPostSuggestionApplicationFirstNavigationHistogram,
        MultistepFilterPostSuggestionApplicationFirstNavigation::
            kForwardOrOtherNavigation,
        1);

    tracker.OnSuggestionApplicationAnnotationExtractionFinished(
        /*was_applied_successfully=*/true);

    histogram_tester.ExpectUniqueSample(
        kMultistepFilterPostSuggestionApplicationTabCloseHistogram,
        MultistepFilterPostSuggestionApplicationTabClose::
            kTabClosedWithFurtherNavigation,
        1);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationTabCloseHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationTabClose::
                            kTabClosedWithinSessionWindow,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationTabClose::
                            kTabClosedWithFurtherNavigation,
                        1)));
}

}  // namespace
}  // namespace multistep_filter
