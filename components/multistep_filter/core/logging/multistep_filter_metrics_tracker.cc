// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

#include <algorithm>
#include <utility>

#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics_util.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace multistep_filter {

namespace {

using SuggestionUiSession = MultistepFilterMetricsTracker::SuggestionUiSession;
using SuggestionApplicationSession = MultistepFilterMetricsTracker::SuggestionApplicationSession;
using SuggestionApplicationSessionFlushTrigger =
    MultistepFilterMetricsTracker::SuggestionApplicationSessionFlushTrigger;
using enum SuggestionApplicationSessionFlushTrigger;
using enum MultistepFilterPostSuggestionApplicationUserEngagement;

static_assert(static_cast<int>(MultistepFilterFacetType::kMaxValue) < 64,
              "MultistepFilterFacetType::kMaxValue must be less than 64 to fit "
              "in a 64-bit bitmask.");

uint64_t GetAppliedFacetTypesBitmask(const UrlFilterSuggestion& suggestion) {
  uint64_t bitmask = 0;
  for (const FilterAttributeUiLabel& label : suggestion.attribute_ui_labels) {
    MultistepFilterFacetType facet_type = MapStringToFacetType(label.key);
    if (facet_type != MultistepFilterFacetType::kUnknown) {
      bitmask |= 1ULL << std::to_underlying(facet_type);
    }
  }
  return bitmask;
}

void LogAcceptanceHistogram(std::string_view base_histogram,
                            std::string_view task_type,
                            SuggestionUserDecision decision,
                            const RetentionStateSnapshot& snapshot) {
  base::UmaHistogramEnumeration(std::string(base_histogram), decision);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {base_histogram, kMultistepFilterByTaskHistogramPrefix, task_type}),
      decision);
  EnumerateActiveRetentionSlices(
      GetRetentionState(snapshot), [&](std::string_view slice) {
        base::UmaHistogramEnumeration(
            base::StrCat({base_histogram,
                          kMultistepFilterByRetentionHistogramPrefix, slice}),
            decision);
      });
}

bool IsTechnicalFailureOutcome(SuggestionApplicationResult outcome) {
  switch (outcome) {
    case SuggestionApplicationResult::kNotAllFiltersApplied:
    case SuggestionApplicationResult::kFailedErrorPage:
    case SuggestionApplicationResult::kFailedNoExtractedAnnotations:
    case SuggestionApplicationResult::kFailedCountMismatch:
    case SuggestionApplicationResult::kFailedAttributeMismatch:
      return true;
    case SuggestionApplicationResult::kAllFiltersApplied:
    case SuggestionApplicationResult::kAbandonedBeforeVerification:
      return false;
  }
}

// Logs the overall technical filter application outcome after a user accepts
// a Multistep Filter suggestion.
// "All filter facets successfully applied" (kAllFiltersApplied) means that
// after the navigation completed, the landing page's URL was successfully
// parsed, and the extracted filter attributes exactly matched the suggested
// filters (same keys and values).
//
// "Not successfully applied" (kNotAllFiltersApplied) covers cases where:
//
// - The navigation failed (error page) or was to an unsupported scheme.
// - No filter attributes could be extracted from the landing page.
// - The number of extracted attributes did not match the suggested count.
// - There was a mismatch in the keys or values of the filters (e.g. a
//   different brand or price range was applied than what was suggested).
void LogApplicationOutcome(
    SuggestionApplicationSession& session) {
  if (session.outcome_logged) {
    return;
  }

  base::UmaHistogramEnumeration(kMultistepFilterApplicationOutcomeHistogram,
                                session.outcome);
  base::UmaHistogramEnumeration(
      base::StrCat({kMultistepFilterApplicationOutcomeHistogram,
                    kMultistepFilterByTaskHistogramPrefix,
                    session.suggestion.task_type}),
      session.outcome);

  // Log by retention state:
  EnumerateActiveRetentionSlices(
      GetRetentionState(session.retention_snapshot),
      [&](std::string_view slice) {
        base::UmaHistogramEnumeration(
            base::StrCat({kMultistepFilterApplicationOutcomeHistogram,
                          kMultistepFilterByRetentionHistogramPrefix, slice}),
            session.outcome);
      });

  const bool is_success =
      session.outcome == SuggestionApplicationResult::kAllFiltersApplied;

  if (is_success) {
    size_t count = session.suggestion.attribute_ui_labels.size();
    base::UmaHistogramCounts100(
        kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, count);
    base::UmaHistogramCounts100(
        base::StrCat(
            {kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram,
             kMultistepFilterByTaskHistogramPrefix,
             session.suggestion.task_type}),
        count);
    base::UmaHistogramMediumTimes(
        kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram,
        session.suggestion_accepted_to_applied_latency);
  }

  for (const FilterAttributeUiLabel& suggested_label :
       session.suggestion.attribute_ui_labels) {
    base::UmaHistogramBoolean(
        base::StrCat({kMultistepFilterApplicationOutcomeHistogram,
                      kMultistepFilterByTaskHistogramPrefix,
                      session.suggestion.task_type,
                      kMultistepFilterByFacetHistogramPrefix,
                      suggested_label.key}),
        is_success);
  }
  session.outcome_logged = true;
}

void LogApplicationOutcomeWhenSessionIsFlushed(
    SuggestionApplicationSession& session,
    SuggestionApplicationSessionFlushTrigger trigger) {
  if (session.outcome_logged) {
    return;
  }

  switch (trigger) {
    case kApplicationFailure:
      // An application failure flush requires that a specific technical failure
      // outcome was set on the session before flushing.
      CHECK(IsTechnicalFailureOutcome(session.outcome));
      break;
    case kTabClosed:
    case kSessionOverride:
    case kNavigationBack:
    case kNavigationFromBrowserContext:
    case kNavigationFromPageContext:
      session.outcome =
          SuggestionApplicationResult::kAbandonedBeforeVerification;
      break;
  }

  LogApplicationOutcome(session);
}

void LogSuggestionUiShown(
    const SuggestionUiSession& ui_session) {
  size_t count = ui_session.suggestion.attribute_ui_labels.size();
  std::string_view task_type = ui_session.suggestion.task_type;
  if (count > 0) {
    base::UmaHistogramCounts100(kMultistepFilterNumberOfFacetsShownHistogram,
                                count);
    base::UmaHistogramCounts100(
        base::StrCat({kMultistepFilterNumberOfFacetsShownHistogram,
                      kMultistepFilterByTaskHistogramPrefix, task_type}),
        count);
  }

  CHECK(!ui_session.triggering_navigation_finish_time.is_null());
  base::UmaHistogramTimes(
      kMultistepFilterTimeNavigationToSuggestionShownHistogram,
      ui_session.navigation_to_suggestion_shown_latency);

  const int suggestion_age_minutes =
      ui_session.extraction_to_suggestion_shown_time_delta.InMinutes();
  const int max_age_bucket =
      kMultistepFilterSessionDuration.Get().InMinutes() + 1;
  base::UmaHistogramExactLinear(kMultistepFilterSuggestionAgeShownHistogram,
                                suggestion_age_minutes, max_age_bucket);
  if (ui_session.is_same_domain) {
    base::UmaHistogramExactLinear(
        kMultistepFilterSuggestionAgeShownOnSameDomainHistogram,
        suggestion_age_minutes, max_age_bucket);
  }
}

void LogSuggestionAcceptanceLatencyAndAgeMetrics(
    const MultistepFilterMetricsTracker::SuggestionUiSession& ui_session) {
  CHECK(!ui_session.triggering_navigation_finish_time.is_null());
  base::UmaHistogramMediumTimes(
      kMultistepFilterTimeNavigationToSuggestionAcceptedHistogram,
      ui_session.navigation_to_suggestion_accepted_time_delta);

  base::UmaHistogramMediumTimes(
      kMultistepFilterTimeSuggestionShownToAcceptedHistogram,
      ui_session.suggestion_shown_to_accepted_time_delta);

  const int suggestion_age_minutes =
      ui_session.extraction_to_suggestion_accepted_time_delta.InMinutes();
  const int max_age_bucket =
      kMultistepFilterSessionDuration.Get().InMinutes() + 1;
  base::UmaHistogramExactLinear(kMultistepFilterSuggestionAgeAcceptedHistogram,
                                suggestion_age_minutes, max_age_bucket);
  if (ui_session.is_same_domain) {
    base::UmaHistogramExactLinear(
        kMultistepFilterSuggestionAgeAcceptedOnSameDomainHistogram,
        suggestion_age_minutes, max_age_bucket);
  }
}

// Helper to determine if a navigation should be ignored for engagement metrics.
bool ShouldIgnoreNavigationForEngagementMetrics(
    const FilterNavigationMetadata& metadata) {
  // Ignore filter-initiated navigations (they are handled as overrides if a new
  // suggestion is applied, and shouldn't count as engagement for the current
  // session).
  if (metadata.was_filter_initiated_navigation) {
    return true;
  }
  // Ignore same-document navigations unless they are back navigations or have
  // user gesture.
  if (metadata.is_same_document_navigation && !metadata.is_back_navigation &&
      !metadata.has_user_gesture) {
    return true;
  }
  // Ignore same-url reloads.
  if (!metadata.url.is_empty() && metadata.url == metadata.prev_url &&
      !metadata.is_same_document_navigation) {
    return true;
  }
  return false;
}


// Returns the post-application user engagement metric for a session.
// `trigger` must not be `kApplicationFailure` because post-application
// engagement is only tracked for successful applications.
MultistepFilterPostSuggestionApplicationUserEngagement
GetPostApplicationUserEngagement(
    const SuggestionApplicationSession& session,
    SuggestionApplicationSessionFlushTrigger trigger,
    base::TimeTicks event_time) {


  CHECK_NE(trigger, kApplicationFailure);

  const base::TimeDelta time_since_suggestion_application_finish =
      GetClampedDifference(event_time,
                           session.application_navigation_finish_time);
  const bool within_window =
      time_since_suggestion_application_finish <
      kMultistepFilterPostApplicationSessionDuration.Get();

  switch (trigger) {
    case kApplicationFailure:
      NOTREACHED();
    case kNavigationFromPageContext:
      return within_window ? kEngagedWithFurtherNavigationWithinSessionWindow
                           : kEngagedWithFurtherNavigationAfterSessionWindow;
    case kTabClosed:
      return within_window ? kAbandonedWithinSessionWindowTabClosed
                           : kAbandonedAfterSessionWindowTabClosed;
    case kNavigationFromBrowserContext:
      return within_window ? kAbandonedWithinSessionWindowOmniboxOrBookmark
                           : kAbandonedAfterSessionWindowOmniboxOrBookmark;
    case kNavigationBack:
      return within_window ? kAbandonedWithinSessionWindowBackNavigation
                           : kAbandonedAfterSessionWindowBackNavigation;
    case kSessionOverride:
      return within_window ? kAbandonedWithinSessionWindowSessionOverride
                           : kAbandonedAfterSessionWindowSessionOverride;
  }
  NOTREACHED();
}

void LogPostApplicationUserEngagement(
    const SuggestionApplicationSession& session,
    SuggestionApplicationSessionFlushTrigger trigger,
    base::TimeTicks event_time) {
  if (trigger == kApplicationFailure) {
    return;
  }
  base::UmaHistogramEnumeration(
      kMultistepFilterPostSuggestionApplicationUserEngagementHistogram,
      GetPostApplicationUserEngagement(session, trigger, event_time));
}

void LogSuggestionUiSessionUkm(
    const SuggestionUiSession& ui_session,
    SuggestionUserDecision final_decision) {
  if (ui_session.ukm_source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::MultistepFilter_UiSession builder(ui_session.ukm_source_id);

  int max_session_duration_minutes =
      kMultistepFilterSessionDuration.Get().InMinutes();

  builder.SetSessionId(ui_session.session_id)
      .SetTaskType(std::to_underlying(
          MapStringToTaskType(ui_session.suggestion.task_type)))
      .SetUserDecision(std::to_underlying(final_decision))
      .SetRetentionState(
          std::to_underlying(GetRetentionState(ui_session.retention_snapshot)))
      .SetShownAgeInMinutes(std::min<int64_t>(
          ui_session.extraction_to_suggestion_shown_time_delta.InMinutes(),
          max_session_duration_minutes))
      .SetNumOfFilterFacetsShown(std::min<int64_t>(
          ui_session.suggestion.attribute_ui_labels.size(),
          kMultistepFilterMaxFacetsShownUkmClampingLimit.Get()))
      .SetReopenedCueShown(ui_session.reopened_cue_shown)
      .SetIsSameDomain(ui_session.is_same_domain)
      .SetSuggestedFilterFacetTypes(
          GetAppliedFacetTypesBitmask(ui_session.suggestion));

  builder.SetNavigationToSuggestionShownTimeInMs(
      ukm::GetSemanticBucketMinForDurationTiming(
          ui_session.navigation_to_suggestion_shown_latency.InMilliseconds()));

  if (final_decision == SuggestionUserDecision::kAccepted) {
    builder.SetAcceptedAgeInMinutes(std::min<int64_t>(
        ui_session.extraction_to_suggestion_accepted_time_delta.InMinutes(),
        max_session_duration_minutes));

    builder.SetNavigationToSuggestionAcceptedTimeInMs(
        ukm::GetSemanticBucketMinForDurationTiming(
            ui_session.navigation_to_suggestion_accepted_time_delta
                .InMilliseconds()));

    builder.SetSuggestionShownToAcceptedTimeInMs(
        ukm::GetSemanticBucketMinForDurationTiming(
            ui_session.suggestion_shown_to_accepted_time_delta
                .InMilliseconds()));
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void LogSuggestionApplicationSessionUkm(
    const SuggestionApplicationSession& session,
    SuggestionApplicationSessionFlushTrigger trigger,
    base::TimeTicks event_time) {
  if (session.ukm_source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::MultistepFilter_ApplicationSession builder(
      session.ukm_source_id);

  builder.SetSessionId(session.session_id)
      .SetTaskType(
          std::to_underlying(MapStringToTaskType(session.suggestion.task_type)))
      .SetRetentionState(
          std::to_underlying(GetRetentionState(session.retention_snapshot)))
      .SetApplicationOutcome(std::to_underlying(session.outcome));

  if (session.outcome == SuggestionApplicationResult::kAllFiltersApplied) {
    builder.SetNumOfFilterFacetsAppliedSuccessfully(std::min<int64_t>(
        session.suggestion.attribute_ui_labels.size(),
        kMultistepFilterMaxFacetsShownUkmClampingLimit.Get()));

    builder.SetSuggestionAcceptedToAppliedTimeInMs(
        ukm::GetSemanticBucketMinForDurationTiming(
            session.suggestion_accepted_to_applied_latency.InMilliseconds()));

    builder.SetSuggestedFilterFacetTypes(
        GetAppliedFacetTypesBitmask(session.suggestion));
  }

  if (trigger != kApplicationFailure) {
    builder.SetPostApplicationUserEngagement(std::to_underlying(
        GetPostApplicationUserEngagement(session, trigger, event_time)));
  }

  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace

MultistepFilterMetricsTracker::MultistepFilterMetricsTracker() = default;

MultistepFilterMetricsTracker::~MultistepFilterMetricsTracker() {
  if (current_ui_session_.has_value()) {
    FlushSuggestionUiSession(SuggestionUserDecision::kIgnored);
  }
  if (current_application_session_.has_value()) {
    FlushSuggestionApplicationSession(
        SuggestionApplicationSessionFlushTrigger::kTabClosed,
        base::TimeTicks::Now());
  }
}

void MultistepFilterMetricsTracker::OnNavigationFinished(
    const FilterNavigationMetadata& metadata) {
  CHECK(!metadata.navigation_start_time.is_null());
  CHECK(!metadata.navigation_finish_time.is_null());
  current_navigation_ = NavigationSession();
  current_navigation_.navigation_finish_time = metadata.navigation_finish_time;
  current_navigation_.ukm_source_id = metadata.ukm_source_id;

  MaybeFlushSessionOnNavigation(metadata);

  // If this navigation is applying the suggestion, start the application
  // session.
  if (metadata.applied_suggestion.has_value()) {
    PendingSuggestionApplicationMetrics pending_metrics =
        pending_suggestion_application_metrics_.value_or(
            PendingSuggestionApplicationMetrics());
    const base::TimeTicks suggestion_accepted_time =
        pending_metrics.suggestion_accepted_time.is_null()
            ? metadata.navigation_start_time
            : pending_metrics.suggestion_accepted_time;
    current_application_session_ = SuggestionApplicationSession{
        .ukm_source_id = metadata.ukm_source_id,
        .session_id = pending_metrics.session_id,
        .suggestion = metadata.applied_suggestion.value(),
        .retention_snapshot = pending_metrics.retention_snapshot,
        .suggestion_accepted_time = suggestion_accepted_time,
        .is_error_page = metadata.is_error_page_navigation,
        .application_navigation_finish_time = metadata.navigation_finish_time,
        .suggestion_accepted_to_applied_latency = GetClampedDifference(
            metadata.navigation_finish_time, suggestion_accepted_time),
    };
    if (current_application_session_->is_error_page) {
      current_application_session_->outcome =
          SuggestionApplicationResult::kFailedErrorPage;
      FlushSuggestionApplicationSession(
          SuggestionApplicationSessionFlushTrigger::kApplicationFailure,
          metadata.navigation_finish_time);
    }
  }
  pending_suggestion_application_metrics_.reset();

  // If a UI session has been started but not concluded yet, the finishing
  // of the navigation indicates that the user ignored the UI.
  if (current_ui_session_.has_value()) {
    bool is_same_page = metadata.is_same_document_navigation ||
                        metadata.url == metadata.prev_url;
    if (!is_same_page) {
      FlushSuggestionUiSession(SuggestionUserDecision::kIgnored);
    } else {
      current_ui_session_->is_preserved_same_page = true;
    }
  }
}

void MultistepFilterMetricsTracker::OnSuggestionShown(
    const UrlFilterSuggestion& suggestion,
    const RetentionStateSnapshot& retention_snapshot) {
  if (current_ui_session_.has_value()) {
    FlushSuggestionUiSession(SuggestionUserDecision::kIgnored);
  }
  const base::TimeTicks suggestion_shown_time = base::TimeTicks::Now();
  current_ui_session_ = SuggestionUiSession{
      .triggering_navigation_finish_time =
          current_navigation_.navigation_finish_time,
      .ukm_source_id = current_navigation_.ukm_source_id,
      .session_id = static_cast<int64_t>(base::RandUint64()),
      .suggestion = suggestion,
      .retention_snapshot = retention_snapshot,
      .suggestion_shown_time = suggestion_shown_time,
      .extraction_to_suggestion_shown_time_delta = GetClampedDifference(
          base::Time::Now(), suggestion.extraction_timestamp),
      .navigation_to_suggestion_shown_latency = GetClampedDifference(
          suggestion_shown_time, current_navigation_.navigation_finish_time),
      .is_same_domain = IsSameEtldPlusOne(suggestion),
      .user_decision = SuggestionUserDecision::kIgnored,
  };
  LogSuggestionUiShown(*current_ui_session_);
}

void MultistepFilterMetricsTracker::OnSuggestionReopened() {
  CHECK(current_ui_session_.has_value());
  current_ui_session_->reopened_cue_shown = true;
}

void MultistepFilterMetricsTracker::OnSuggestionUserInteraction(
    SuggestionUserDecision decision) {
  if (!current_ui_session_.has_value()) {
    // This can happen due to race conditions (e.g. user clicks action just as
    // navigation clears the suggestion) or system-initiated clears.
    return;
  }
  switch (decision) {
    case SuggestionUserDecision::kAccepted:
      current_ui_session_->suggestion_accepted_time = base::TimeTicks::Now();
      current_ui_session_->navigation_to_suggestion_accepted_time_delta =
          GetClampedDifference(
              current_ui_session_->suggestion_accepted_time,
              current_ui_session_->triggering_navigation_finish_time);
      current_ui_session_->suggestion_shown_to_accepted_time_delta =
          GetClampedDifference(current_ui_session_->suggestion_accepted_time,
                               current_ui_session_->suggestion_shown_time);
      current_ui_session_->extraction_to_suggestion_accepted_time_delta =
          GetClampedDifference(
              base::Time::Now(),
              current_ui_session_->suggestion.extraction_timestamp);
      pending_suggestion_application_metrics_ =
          PendingSuggestionApplicationMetrics{
              .retention_snapshot = current_ui_session_->retention_snapshot,
              .session_id = current_ui_session_->session_id,
              .suggestion_accepted_time =
                  current_ui_session_->suggestion_accepted_time,
          };
      break;
    case SuggestionUserDecision::kDismissed:
    case SuggestionUserDecision::kSettingsOpened:
    case SuggestionUserDecision::kIgnored:
      break;
  }
  current_ui_session_->user_decision = decision;
  FlushSuggestionUiSession(decision);
}

void MultistepFilterMetricsTracker::OnPreservedSuggestionCleared() {
  if (current_ui_session_.has_value() &&
      current_ui_session_->is_preserved_same_page) {
    FlushSuggestionUiSession(SuggestionUserDecision::kIgnored);
  }
}

void MultistepFilterMetricsTracker::OnSuggestionApplicationFinished(
    SuggestionApplicationResult result) {
  if (!current_application_session_.has_value()) {
    return;
  }
  current_application_session_->outcome = result;
  current_application_session_->is_applied =
      result == SuggestionApplicationResult::kAllFiltersApplied;
  if (current_application_session_->is_applied) {
    LogApplicationOutcome(*current_application_session_);
  } else {
    // In case of application failure, flush the session immediately.
    FlushSuggestionApplicationSession(
        SuggestionApplicationSessionFlushTrigger::kApplicationFailure,
        base::TimeTicks::Now());
  }
}

void MultistepFilterMetricsTracker::FlushSuggestionUiSession(
    SuggestionUserDecision final_decision) {
  CHECK(current_ui_session_.has_value());
  const std::string& task_type = current_ui_session_->suggestion.task_type;
  const RetentionStateSnapshot& snapshot =
      current_ui_session_->retention_snapshot;

  // If the reopened cue was shown, the initial cue was ignored.
  SuggestionUserDecision initial_decision =
      current_ui_session_->reopened_cue_shown ? SuggestionUserDecision::kIgnored
                                              : final_decision;
  LogAcceptanceHistogram(kMultistepFilterAcceptanceInitialCueHistogram,
                         task_type, initial_decision, snapshot);

  // If the reopened cue was shown, log the final decision to the reopened cue
  // histogram.
  if (current_ui_session_->reopened_cue_shown) {
    LogAcceptanceHistogram(kMultistepFilterAcceptanceReopenedCueHistogram,
                           task_type, final_decision, snapshot);
  }

  LogAcceptanceHistogram(kMultistepFilterAcceptanceHistogram, task_type,
                         final_decision, snapshot);
  if (final_decision == SuggestionUserDecision::kAccepted) {
    LogSuggestionAcceptanceLatencyAndAgeMetrics(*current_ui_session_);
  }
  LogSuggestionUiSessionUkm(*current_ui_session_, final_decision);
  current_ui_session_ = std::nullopt;
}

void MultistepFilterMetricsTracker::FlushSuggestionApplicationSession(
    SuggestionApplicationSessionFlushTrigger trigger,
    base::TimeTicks event_time) {
  CHECK(current_application_session_.has_value());
  LogApplicationOutcomeWhenSessionIsFlushed(*current_application_session_,
                                            trigger);

  LogPostApplicationUserEngagement(*current_application_session_, trigger,
                                   event_time);
  LogSuggestionApplicationSessionUkm(*current_application_session_, trigger,
                                     event_time);
  current_application_session_ = std::nullopt;
}

void MultistepFilterMetricsTracker::MaybeFlushSessionOnNavigation(
    const FilterNavigationMetadata& metadata) {
  if (!current_application_session_.has_value()) {
    return;
  }

  // If this navigation is applying a new suggestion, it overrides the active
  // one.
  if (metadata.applied_suggestion.has_value()) {
    FlushSuggestionApplicationSession(kSessionOverride,
                                      metadata.navigation_start_time);
    return;
  }

  // Ignore same-document navigations or reloads in all phases.
  if (ShouldIgnoreNavigationForEngagementMetrics(metadata)) {
    return;
  }

  SuggestionApplicationSessionFlushTrigger trigger;
  if (metadata.is_navigation_from_omnibox_or_bookmarks) {
    trigger = kNavigationFromBrowserContext;
  } else if (metadata.is_back_navigation) {
    trigger = kNavigationBack;
  } else {
    trigger = kNavigationFromPageContext;
  }
  FlushSuggestionApplicationSession(trigger, metadata.navigation_start_time);
}

}  // namespace multistep_filter
