// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

#include <algorithm>
#include <utility>

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
#include "components/multistep_filter/core/verification/suggestion_application_result.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
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
  metadata.ukm_source_id = ukm::UkmRecorder::GetNewSourceID();
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
  tracker.OnSuggestionApplicationFinished(
      SuggestionApplicationResult::kAllFiltersApplied);
}

UrlFilterSuggestion CreateSuggestionWithAttributes(
    std::string task_type,
    std::vector<std::pair<std::string, std::string>> attrs) {
  UrlFilterSuggestion::Params params;
  params.navigation_url = GURL("https://example.com");
  params.source_host = u"source.com";
  params.triggering_host = "trigger.com";
  params.task_type = std::move(task_type);
  params.extraction_timestamp = base::Time::Now();
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
    SuggestionApplicationResult expected_outcome,
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
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  ukm::SourceId expected_source_id = ukm::kInvalidSourceId;
  {
    MultistepFilterMetricsTracker tracker;
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    expected_source_id = metadata.ukm_source_id;
    tracker.OnNavigationFinished(metadata);
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

  // Verify UKM.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::MultistepFilter_UiSession::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(expected_source_id, entry->source_id);
  const int64_t* session_id_metric = ukm_recorder.GetEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kSessionIdName);
  ASSERT_NE(nullptr, session_id_metric);
  EXPECT_NE(0, *session_id_metric);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kTaskTypeName,
      std::to_underlying(MultistepFilterTaskType::kSearchFlights));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kUserDecisionName,
      std::to_underlying(SuggestionUserDecision::kAccepted));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kRetentionStateName,
      std::to_underlying(MultistepFilterRetentionState::kFirstImpression));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kShownAgeInMinutesName,
      10);
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_UiSession::kAcceptedAgeInMinutesName, 15);
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_UiSession::kNumOfFilterFacetsShownName, 0);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kReopenedCueShownName,
      1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kIsSameDomainName, 1);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm::builders::MultistepFilter_UiSession::
                                     kNavigationToSuggestionShownTimeInMsName,
                                 300);
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_UiSession::
          kNavigationToSuggestionAcceptedTimeInMsName,
      300000);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm::builders::MultistepFilter_UiSession::
                                     kSuggestionShownToAcceptedTimeInMsName,
                                 300000);
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
  {
    MultistepFilterMetricsTracker tracker;
    task_environment_.FastForwardBy(base::Milliseconds(2000));
    metadata.navigation_finish_time = base::TimeTicks::Now();
    tracker.OnNavigationFinished(metadata);
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kAllFiltersApplied);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(Bucket(SuggestionApplicationResult::kAllFiltersApplied, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
      BucketsAre(Bucket(SuggestionApplicationResult::kAllFiltersApplied, 1)));
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kAllFiltersApplied);
  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram,
      base::Milliseconds(2000), 1);
}

// Verifies that suggestion application latency is calculated using the precise
// browser-side acceptance time, even if there is a delay before the landing
// page navigation starts.
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionApplicationSuccessUsesPreciseAcceptanceTime) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  MultistepFilterMetricsTracker tracker;
  TriggerInitialNavigation(tracker);
  tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
  tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
  task_environment_.FastForwardBy(base::Milliseconds(500));
  FilterNavigationMetadata metadata;
  metadata.applied_suggestion = suggestion;
  metadata.navigation_start_time = base::TimeTicks::Now();
  task_environment_.FastForwardBy(base::Milliseconds(1500));
  metadata.navigation_finish_time = base::TimeTicks::Now();
  tracker.OnNavigationFinished(metadata);
  tracker.OnSuggestionApplicationFinished(
      SuggestionApplicationResult::kAllFiltersApplied);
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
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kFailedNoExtractedAnnotations);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            kMultistepFilterApplicationOutcomeHistogram),
        BucketsAre(Bucket(
            SuggestionApplicationResult::kFailedNoExtractedAnnotations, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
        BucketsAre(Bucket(
            SuggestionApplicationResult::kFailedNoExtractedAnnotations, 1)));
    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester, {kRetentionSliceFirstImpression},
        SuggestionApplicationResult::kFailedNoExtractedAnnotations);
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
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kAllFiltersApplied);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            kMultistepFilterApplicationOutcomeHistogram),
        BucketsAre(Bucket(SuggestionApplicationResult::kFailedErrorPage, 1)));
    histogram_tester.ExpectTotalCount(
        kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram, 0);
  }
}

// Tests that if navigation lands on an error page, the session is flushed
// immediately as a technical failure (kNotAllFiltersApplied) and no engagement
// is logged even if the tab is closed.
TEST_F(MultistepFilterMetricsTrackerTest,
       ErrorPageNavigationFlushesImmediately) {
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  {
    MultistepFilterMetricsTracker tracker;
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.applied_suggestion = suggestion;
    metadata.is_error_page_navigation = true;
    metadata.ukm_source_id = 123;
    tracker.OnNavigationFinished(metadata);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            kMultistepFilterApplicationOutcomeHistogram),
        BucketsAre(Bucket(SuggestionApplicationResult::kFailedErrorPage, 1)));
  }
  histogram_tester.ExpectTotalCount(
      kMultistepFilterPostSuggestionApplicationUserEngagementHistogram, 0);

  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::MultistepFilter_ApplicationSession::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::
          kApplicationOutcomeName,
      std::to_underlying(SuggestionApplicationResult::kFailedErrorPage));
}

// Tests that calling annotation extraction finished without a session records
// no samples.
TEST_F(MultistepFilterMetricsTrackerTest,
       ApplicationFinishedWithoutSessionRecordsNoSamples) {
  base::HistogramTester histogram_tester;
  MultistepFilterMetricsTracker tracker;
  tracker.OnSuggestionApplicationFinished(
      SuggestionApplicationResult::kAllFiltersApplied);

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
      BucketsAre(Bucket(
          SuggestionApplicationResult::kAbandonedBeforeVerification, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
      BucketsAre(Bucket(
          SuggestionApplicationResult::kAbandonedBeforeVerification, 1)));
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kAbandonedBeforeVerification);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kEngagedWithFurtherNavigationWithinSessionWindow,
                        1)));
}

// Tests that if the tab is closed (tracker destroyed) while we were waiting
// for extraction of a previously applied suggestion, that application session
// is flushed as abandoned before verification.
TEST_F(MultistepFilterMetricsTrackerTest,
       ApplicationInterruptedByTabCloseRecordsAbandoned) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  {
    MultistepFilterMetricsTracker tracker;
    FilterNavigationMetadata landing_metadata = CreateDefaultMetadata();
    landing_metadata.applied_suggestion = suggestion;
    tracker.OnNavigationFinished(landing_metadata);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(Bucket(
          SuggestionApplicationResult::kAbandonedBeforeVerification, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS"),
      BucketsAre(Bucket(
          SuggestionApplicationResult::kAbandonedBeforeVerification, 1)));
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kAbandonedBeforeVerification);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowTabClosed,
                        1)));
}

// Tests that if a navigation is interrupted by a back navigation before
// extraction finishes, we log the back navigation engagement even though
// it wasn't verified.
TEST_F(MultistepFilterMetricsTrackerTest,
       ApplicationInterruptedByBackNavigationRecordsEngagement) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  MultistepFilterMetricsTracker tracker;
  FilterNavigationMetadata landing_metadata = CreateDefaultMetadata();
  landing_metadata.applied_suggestion = suggestion;
  tracker.OnNavigationFinished(landing_metadata);
  FilterNavigationMetadata interrupt_metadata = CreateDefaultMetadata();
  interrupt_metadata.is_back_navigation = true;
  tracker.OnNavigationFinished(interrupt_metadata);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(Bucket(
          SuggestionApplicationResult::kAbandonedBeforeVerification, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowBackNavigation,
                        1)));
}

// Tests that showing a suggestion logs the number of facets shown.
TEST_F(MultistepFilterMetricsTrackerTest, FacetsShownLogged) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      kMultistepFilterTaskTypeSearchAccommodations,
      {{kMultistepFilterFacetTypeDateCheckin, "2026-07-22"},
       {kMultistepFilterFacetTypeLocationDestination, "Paris"}});
  ukm::SourceId expected_source_id = ukm::kInvalidSourceId;
  {
    MultistepFilterMetricsTracker tracker;
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    expected_source_id = metadata.ukm_source_id;
    tracker.OnNavigationFinished(metadata);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
  }
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsShownHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsShown.ByTask.SEARCH_ACCOMMODATIONS", 2, 1);

  // Verify UKM.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::MultistepFilter_UiSession::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(expected_source_id, entry->source_id);
  const int64_t* session_id_metric = ukm_recorder.GetEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kSessionIdName);
  ASSERT_NE(nullptr, session_id_metric);
  EXPECT_NE(0, *session_id_metric);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kTaskTypeName,
      std::to_underlying(MultistepFilterTaskType::kSearchAccommodations));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kUserDecisionName,
      std::to_underlying(SuggestionUserDecision::kIgnored));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kRetentionStateName,
      std::to_underlying(MultistepFilterRetentionState::kFirstImpression));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kShownAgeInMinutesName,
      0);
  EXPECT_EQ(
      nullptr,
      ukm_recorder.GetEntryMetric(
          entry,
          ukm::builders::MultistepFilter_UiSession::kAcceptedAgeInMinutesName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_UiSession::kNumOfFilterFacetsShownName, 2);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kReopenedCueShownName,
      0);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_UiSession::kIsSameDomainName, 0);
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_UiSession::kSuggestedFilterFacetTypesName,
      34816);  // (1ULL << 11) | (1ULL << 15)
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm::builders::MultistepFilter_UiSession::
                                     kNavigationToSuggestionShownTimeInMsName,
                                 0);
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entry, ukm::builders::MultistepFilter_UiSession::
                           kNavigationToSuggestionAcceptedTimeInMsName));
  EXPECT_EQ(nullptr, ukm_recorder.GetEntryMetric(
                         entry, ukm::builders::MultistepFilter_UiSession::
                                    kSuggestionShownToAcceptedTimeInMsName));
}

// Tests that successful suggestion application logs the count of successfully
// applied facets and per-facet success.
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionApplicationSuccessLogsFacetCountAndOutcomes) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS",
      {{"DATE_CHECKIN", "2026-07-20"}, {"LOCATION_DESTINATION", "Paris"}});
  FilterNavigationMetadata metadata = CreateDefaultMetadata();
  metadata.applied_suggestion = suggestion;
  ukm::SourceId expected_source_id = metadata.ukm_source_id;

  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnNavigationFinished(metadata);
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kAllFiltersApplied);
  }

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsSuccessfullyApplied.ByTask.SEARCH_"
      "ACCOMMODATIONS",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "DATE_CHECKIN",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "LOCATION_DESTINATION",
      true, 1);

  // Verify UKM.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::MultistepFilter_ApplicationSession::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(expected_source_id, entry->source_id);
  EXPECT_NE(
      nullptr,
      ukm_recorder.GetEntryMetric(
          entry,
          ukm::builders::MultistepFilter_ApplicationSession::kSessionIdName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_ApplicationSession::kTaskTypeName,
      std::to_underlying(MultistepFilterTaskType::kSearchAccommodations));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::kRetentionStateName,
      std::to_underlying(MultistepFilterRetentionState::kFirstImpression));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::
          kApplicationOutcomeName,
      std::to_underlying(SuggestionApplicationResult::kAllFiltersApplied));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::
          kNumOfFilterFacetsAppliedSuccessfullyName,
      2);
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::
          kSuggestionAcceptedToAppliedTimeInMsName,
      0);
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::
          kSuggestedFilterFacetTypesName,
      34816);  // (1ULL << 11) | (1ULL << 15)
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::
          kPostApplicationUserEngagementName,
      std::to_underlying(
          MultistepFilterPostSuggestionApplicationUserEngagement::
              kAbandonedWithinSessionWindowTabClosed));
}

// Tests that failed suggestion application logs per-facet failure and no
// success count.
TEST_F(MultistepFilterMetricsTrackerTest,
       SuggestionApplicationFailureLogsPerFacetOutcomes) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  UrlFilterSuggestion suggestion = CreateSuggestionWithAttributes(
      "SEARCH_ACCOMMODATIONS",
      {{"DATE_CHECKIN", "2026-07-20"}, {"LOCATION_DESTINATION", "Paris"}});
  FilterNavigationMetadata metadata = CreateDefaultMetadata();
  metadata.applied_suggestion = suggestion;
  ukm::SourceId expected_source_id = metadata.ukm_source_id;

  {
    MultistepFilterMetricsTracker tracker;
    tracker.OnNavigationFinished(metadata);
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kFailedNoExtractedAnnotations);
  }

  histogram_tester.ExpectTotalCount(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 0);

  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "DATE_CHECKIN",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.SEARCH_ACCOMMODATIONS.ByFacet."
      "LOCATION_DESTINATION",
      false, 1);

  // Verify UKM.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::MultistepFilter_ApplicationSession::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(expected_source_id, entry->source_id);
  EXPECT_NE(
      nullptr,
      ukm_recorder.GetEntryMetric(
          entry,
          ukm::builders::MultistepFilter_ApplicationSession::kSessionIdName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::MultistepFilter_ApplicationSession::kTaskTypeName,
      std::to_underlying(MultistepFilterTaskType::kSearchAccommodations));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::kRetentionStateName,
      std::to_underlying(MultistepFilterRetentionState::kFirstImpression));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::MultistepFilter_ApplicationSession::
          kApplicationOutcomeName,
      std::to_underlying(
          SuggestionApplicationResult::kFailedNoExtractedAnnotations));
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entry, ukm::builders::MultistepFilter_ApplicationSession::
                           kNumOfFilterFacetsAppliedSuccessfullyName));
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entry, ukm::builders::MultistepFilter_ApplicationSession::
                           kSuggestionAcceptedToAppliedTimeInMsName));
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entry, ukm::builders::MultistepFilter_ApplicationSession::
                           kSuggestedFilterFacetTypesName));
  EXPECT_EQ(nullptr,
            ukm_recorder.GetEntryMetric(
                entry, ukm::builders::MultistepFilter_ApplicationSession::
                           kPostApplicationUserEngagementName));
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
    {
      MultistepFilterMetricsTracker tracker;
      TriggerInitialNavigation(tracker);
      tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());
      tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
      tracker.OnNavigationFinished(landing_metadata);
      tracker.OnSuggestionApplicationFinished(
          SuggestionApplicationResult::kAllFiltersApplied);
    }

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester, {kRetentionSliceFirstImpression},
        SuggestionApplicationResult::kAllFiltersApplied);
  }

  // Scenario 2: AcceptedLastTime + AcceptedAtLeastOnce.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 1;
    snapshot.suggestion_acceptances = 1;
    snapshot.is_last_suggestion_accepted = true;

    base::HistogramTester histogram_tester;
    {
      MultistepFilterMetricsTracker tracker;
      TriggerInitialNavigation(tracker);
      tracker.OnSuggestionShown(suggestion, snapshot);
      tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
      tracker.OnNavigationFinished(landing_metadata);
      tracker.OnSuggestionApplicationFinished(
          SuggestionApplicationResult::kAllFiltersApplied);
    }

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester,
        {kRetentionSliceAcceptedLastTime, kRetentionSliceAcceptedAtLeastOnce},
        SuggestionApplicationResult::kAllFiltersApplied);
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
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kFailedNoExtractedAnnotations);

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester,
        {kRetentionSliceRejectedLastTime,
         kRetentionSliceSawCuesButNeverAccepted},
        SuggestionApplicationResult::kFailedNoExtractedAnnotations);
  }

  // Scenario 4: RejectedLastTime + AcceptedAtLeastOnce.
  {
    RetentionStateSnapshot snapshot;
    snapshot.suggestion_impressions = 2;
    snapshot.suggestion_acceptances = 1;
    snapshot.is_last_suggestion_accepted = false;

    base::HistogramTester histogram_tester;
    {
      MultistepFilterMetricsTracker tracker;
      TriggerInitialNavigation(tracker);
      tracker.OnSuggestionShown(suggestion, snapshot);
      tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
      tracker.OnNavigationFinished(landing_metadata);
      tracker.OnSuggestionApplicationFinished(
          SuggestionApplicationResult::kAllFiltersApplied);
    }

    VerifyApplicationOutcomeRetentionHistograms(
        histogram_tester,
        {kRetentionSliceRejectedLastTime, kRetentionSliceAcceptedAtLeastOnce},
        SuggestionApplicationResult::kAllFiltersApplied);
  }
}

TEST_F(MultistepFilterMetricsTrackerTest,
       ApplicationRetentionSnapshotResetOnInterveningNavigation) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("task_type");
  RetentionStateSnapshot snapshot;
  snapshot.suggestion_impressions = 2;
  snapshot.suggestion_acceptances = 1;
  snapshot.is_last_suggestion_accepted = true;

  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, snapshot);

    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);

    FilterNavigationMetadata normal_metadata = CreateDefaultMetadata();
    normal_metadata.url = GURL("https://example.com/normal");
    tracker.OnNavigationFinished(normal_metadata);

    FilterNavigationMetadata landing_metadata = CreateDefaultMetadata();
    landing_metadata.url = GURL("https://example.com/landing");
    landing_metadata.applied_suggestion = suggestion;
    tracker.OnNavigationFinished(landing_metadata);
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kAllFiltersApplied);
  }
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kAllFiltersApplied);
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

// Tests that same page navigations do not flush the active UI session, and that
// suggestion acceptance latency is calculated from the triggering navigation.
TEST_F(MultistepFilterMetricsTrackerTest,
       SamePageNavigationsDoNotFlushUiSession) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());

    task_environment_.FastForwardBy(base::Milliseconds(100));
    FilterNavigationMetadata same_doc_metadata = CreateDefaultMetadata();
    same_doc_metadata.url = GURL("https://example.com/landing#hash");
    same_doc_metadata.prev_url = GURL("https://example.com/landing");
    same_doc_metadata.is_same_document_navigation = true;
    tracker.OnNavigationFinished(same_doc_metadata);

    task_environment_.FastForwardBy(base::Milliseconds(300));
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
  }

  {
    MultistepFilterMetricsTracker tracker;
    TriggerInitialNavigation(tracker);
    tracker.OnSuggestionShown(suggestion, RetentionStateSnapshot());

    task_environment_.FastForwardBy(base::Milliseconds(100));
    FilterNavigationMetadata reload_metadata = CreateDefaultMetadata();
    reload_metadata.url = GURL("https://example.com");
    reload_metadata.prev_url = GURL("https://example.com");
    tracker.OnNavigationFinished(reload_metadata);

    task_environment_.FastForwardBy(base::Milliseconds(300));
    tracker.OnSuggestionUserInteraction(SuggestionUserDecision::kAccepted);
  }

  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kAccepted, 2);
  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeNavigationToSuggestionAcceptedHistogram,
      base::Milliseconds(400), 2);

  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeSuggestionShownToAcceptedHistogram,
      base::Milliseconds(400), 2);
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

  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Minutes(3));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://example.com/other");
    metadata.prev_url = GURL("https://example.com/landing");
    tracker.OnNavigationFinished(metadata);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowBackNavigation,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedAfterSessionWindowBackNavigation,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kEngagedWithFurtherNavigationWithinSessionWindow,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kEngagedWithFurtherNavigationAfterSessionWindow,
                        1)));
}

// Tests that tab abandonment (via tab close) is logged correctly.
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
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowTabClosed,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedAfterSessionWindowTabClosed,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kEngagedWithFurtherNavigationWithinSessionWindow,
                        1)));
}

// Tests that tab abandonment via navigation from omnibox or bookmarks is logged
// correctly.
TEST_F(MultistepFilterMetricsTrackerTest,
       PostAcceptanceNavigationFromOmniboxOrBookmark) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");

  // Navigating away within window, no prior navigation.
  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://google.com");
    metadata.is_navigation_from_omnibox_or_bookmarks = true;
    tracker.OnNavigationFinished(metadata);
  }

  // Navigating away after window, no prior navigation.
  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Minutes(3));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://google.com");
    metadata.is_navigation_from_omnibox_or_bookmarks = true;
    tracker.OnNavigationFinished(metadata);
  }

  // Navigating away within window, with prior navigation.
  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata metadata = CreateDefaultMetadata();
    metadata.url = GURL("https://example.com/other");
    metadata.prev_url = GURL("https://example.com/landing");
    tracker.OnNavigationFinished(metadata);
    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata away_metadata = CreateDefaultMetadata();
    away_metadata.url = GURL("https://google.com");
    away_metadata.is_navigation_from_omnibox_or_bookmarks = true;
    tracker.OnNavigationFinished(away_metadata);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowOmniboxOrBookmark,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedAfterSessionWindowOmniboxOrBookmark,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kEngagedWithFurtherNavigationWithinSessionWindow,
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
    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kFailedNoExtractedAnnotations);

    task_environment_.FastForwardBy(base::Seconds(10));
    FilterNavigationMetadata back_metadata = CreateDefaultMetadata();
    back_metadata.url = GURL("https://example.com/source");
    back_metadata.prev_url = GURL("https://example.com/landing");
    back_metadata.is_back_navigation = true;
    tracker.OnNavigationFinished(back_metadata);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowBackNavigation,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowTabClosed,
                        1)));
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
      kMultistepFilterPostSuggestionApplicationUserEngagementHistogram,
      MultistepFilterPostSuggestionApplicationUserEngagement::
          kAbandonedWithinSessionWindowBackNavigation,
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
    landing_metadata2.was_filter_initiated_navigation = true;
    tracker.OnNavigationFinished(landing_metadata2);

    tracker.OnSuggestionApplicationFinished(
        SuggestionApplicationResult::kAllFiltersApplied);

    histogram_tester.ExpectUniqueSample(
        kMultistepFilterPostSuggestionApplicationUserEngagementHistogram,
        MultistepFilterPostSuggestionApplicationUserEngagement::
            kAbandonedWithinSessionWindowSessionOverride,
        1);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterPostSuggestionApplicationUserEngagementHistogram),
      BucketsAre(Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowTabClosed,
                        1),
                 Bucket(MultistepFilterPostSuggestionApplicationUserEngagement::
                            kAbandonedWithinSessionWindowSessionOverride,
                        1)));
}

// Tests that the session ID logged in MultistepFilter_UiSession matches the
// session ID logged in MultistepFilter_ApplicationSession for the same session.
TEST_F(MultistepFilterMetricsTrackerTest, SessionIdsMatch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  UrlFilterSuggestion suggestion = CreateSuggestion("SEARCH_ACCOMMODATIONS");
  {
    MultistepFilterMetricsTracker tracker;
    SetupPostSuggestionApplicationSession(tracker, suggestion);
  }

  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> ui_entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::MultistepFilter_UiSession::kEntryName);
  ASSERT_EQ(1u, ui_entries.size());
  const int64_t* ui_session_id = ukm_recorder.GetEntryMetric(
      ui_entries[0], ukm::builders::MultistepFilter_UiSession::kSessionIdName);
  ASSERT_NE(nullptr, ui_session_id);
  EXPECT_NE(0, *ui_session_id);

  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> app_entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::MultistepFilter_ApplicationSession::kEntryName);
  ASSERT_EQ(1u, app_entries.size());
  const int64_t* app_session_id = ukm_recorder.GetEntryMetric(
      app_entries[0],
      ukm::builders::MultistepFilter_ApplicationSession::kSessionIdName);
  ASSERT_NE(nullptr, app_session_id);

  EXPECT_EQ(*ui_session_id, *app_session_id);
}

}  // namespace
}  // namespace multistep_filter

