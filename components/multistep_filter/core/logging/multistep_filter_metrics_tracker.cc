// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"

namespace multistep_filter {

namespace {

bool IsSameEtldPlusOne(const UrlFilterSuggestion& suggestion) {
  return GetEtldPlusOneForHost(base::UTF16ToUTF8(suggestion.source_host)) ==
         GetEtldPlusOneForHost(suggestion.triggering_host);
}

base::TimeDelta GetClampedDifference(base::TimeTicks end,
                                     base::TimeTicks start) {
  if (end.is_null() || start.is_null() || end < start) {
    return base::TimeDelta();
  }
  return end - start;
}

base::TimeDelta GetClampedDifference(base::Time end, base::Time start) {
  if (end.is_null() || start.is_null() || end < start) {
    return base::TimeDelta();
  }
  return end - start;
}

// A struct containing the retention slices that should be logged for a given
// snapshot.
struct RetentionSlices {
  bool first_impression = false;
  bool accepted_last_time = false;
  bool rejected_last_time = false;
  bool accepted_at_least_once = false;
  bool saw_cues_but_never_accepted = false;

  // Calls the given callback for each retention slice that should be logged.
  void ForEachActive(base::FunctionRef<void(std::string_view)> callback) const {
    if (first_impression) {
      callback(kRetentionSliceFirstImpression);
    }
    if (accepted_last_time) {
      callback(kRetentionSliceAcceptedLastTime);
    }
    if (rejected_last_time) {
      callback(kRetentionSliceRejectedLastTime);
    }
    if (accepted_at_least_once) {
      callback(kRetentionSliceAcceptedAtLeastOnce);
    }
    if (saw_cues_but_never_accepted) {
      callback(kRetentionSliceSawCuesButNeverAccepted);
    }
  }
};

RetentionSlices GetRetentionSlices(const RetentionStateSnapshot& snapshot) {
  RetentionSlices slices;
  if (snapshot.suggestion_impressions == 0) {
    slices.first_impression = true;
    return slices;
  }
  if (snapshot.is_last_suggestion_accepted) {
    slices.accepted_last_time = true;
  } else {
    slices.rejected_last_time = true;
  }
  if (snapshot.suggestion_acceptances > 0) {
    slices.accepted_at_least_once = true;
  } else {
    slices.saw_cues_but_never_accepted = true;
  }
  return slices;
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
  GetRetentionSlices(snapshot).ForEachActive([&](std::string_view slice) {
    base::UmaHistogramEnumeration(
        base::StrCat({base_histogram,
                      kMultistepFilterByRetentionHistogramPrefix, slice}),
        decision);
  });
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
    const MultistepFilterMetricsTracker::SuggestionApplicationSession&
        application_session,
    const MultistepFilterMetricsTracker::NavigationSession& navigation_session,
    bool is_success) {
  MultistepFilterApplicationOutcome outcome =
      is_success ? MultistepFilterApplicationOutcome::kAllFiltersApplied
                 : MultistepFilterApplicationOutcome::kNotAllFiltersApplied;
  base::UmaHistogramEnumeration(kMultistepFilterApplicationOutcomeHistogram,
                                outcome);
  base::UmaHistogramEnumeration(
      base::StrCat({kMultistepFilterApplicationOutcomeHistogram,
                    kMultistepFilterByTaskHistogramPrefix,
                    application_session.suggestion.task_type}),
      outcome);

  // Log by retention state:
  GetRetentionSlices(application_session.retention_snapshot)
      .ForEachActive([&](std::string_view slice) {
        base::UmaHistogramEnumeration(
            base::StrCat({kMultistepFilterApplicationOutcomeHistogram,
                          kMultistepFilterByRetentionHistogramPrefix, slice}),
            outcome);
      });

  if (is_success) {
    size_t count = application_session.suggestion.attribute_ui_labels.size();
    base::UmaHistogramCounts100(
        kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, count);
    base::UmaHistogramCounts100(
        base::StrCat(
            {kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram,
             kMultistepFilterByTaskHistogramPrefix,
             application_session.suggestion.task_type}),
        count);
    base::UmaHistogramMediumTimes(
        kMultistepFilterTimeSuggestionAcceptanceToAppliedHistogram,
        GetClampedDifference(navigation_session.navigation_finish_time,
                             application_session.suggestion_accepted_time));
  }

  for (const FilterAttributeUiLabel& suggested_label :
       application_session.suggestion.attribute_ui_labels) {
    base::UmaHistogramBoolean(
        base::StrCat({kMultistepFilterApplicationOutcomeHistogram,
                      kMultistepFilterByTaskHistogramPrefix,
                      application_session.suggestion.task_type,
                      kMultistepFilterByFacetHistogramPrefix,
                      suggested_label.key}),
        is_success);
  }
}

void LogSuggestionUiShown(
    const MultistepFilterMetricsTracker::SuggestionUiSession& ui_session) {
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
      GetClampedDifference(ui_session.suggestion_shown_time,
                           ui_session.triggering_navigation_finish_time));

  base::TimeDelta suggestion_age = GetClampedDifference(
      base::Time::Now(), ui_session.suggestion.extraction_timestamp);
  int suggestion_age_minutes = suggestion_age.InMinutes();
  int max_age_bucket = kMultistepFilterSessionDuration.Get().InMinutes() + 1;
  base::UmaHistogramExactLinear(kMultistepFilterSuggestionAgeShownHistogram,
                                suggestion_age_minutes, max_age_bucket);
  if (IsSameEtldPlusOne(ui_session.suggestion)) {
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
      GetClampedDifference(ui_session.suggestion_accepted_time,
                           ui_session.triggering_navigation_finish_time));

  base::UmaHistogramMediumTimes(
      kMultistepFilterTimeSuggestionShownToAcceptedHistogram,
      GetClampedDifference(ui_session.suggestion_accepted_time,
                           ui_session.suggestion_shown_time));

  base::TimeDelta suggestion_age = GetClampedDifference(
      base::Time::Now(), ui_session.suggestion.extraction_timestamp);
  int suggestion_age_minutes = suggestion_age.InMinutes();
  int max_age_bucket = kMultistepFilterSessionDuration.Get().InMinutes() + 1;
  base::UmaHistogramExactLinear(kMultistepFilterSuggestionAgeAcceptedHistogram,
                                suggestion_age_minutes, max_age_bucket);
  if (IsSameEtldPlusOne(ui_session.suggestion)) {
    base::UmaHistogramExactLinear(
        kMultistepFilterSuggestionAgeAcceptedOnSameDomainHistogram,
        suggestion_age_minutes, max_age_bucket);
  }
}

// Helper to determine if a navigation should be ignored for post-application
// metrics.
bool ShouldIgnoreNavigationForPostApplicationMetrics(
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
  if (metadata.url == metadata.prev_url &&
      !metadata.is_same_document_navigation) {
    return true;
  }
  return false;
}

}  // namespace

MultistepFilterMetricsTracker::MultistepFilterMetricsTracker() = default;

MultistepFilterMetricsTracker::~MultistepFilterMetricsTracker() {
  if (current_ui_session_.has_value()) {
    FlushSuggestionUiSession(SuggestionUserDecision::kIgnored);
  }
  if (current_suggestion_application_session_.has_value()) {
    FlushSuggestionApplicationSession(/*was_applied_successfully=*/false);
  }
  if (current_post_suggestion_application_session_.has_value()) {
    FlushPostSuggestionApplicationSession(SessionOutcomeTrigger::kTabClosed,
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

  // If a new navigation finished while we were still waiting for extraction
  // of a previously applied suggestion, that application session is considered
  // failed.
  if (current_suggestion_application_session_.has_value()) {
    FlushSuggestionApplicationSession(/*was_applied_successfully=*/false);
  }

  // If a post-acceptance session is active, track this navigation.
  if (current_post_suggestion_application_session_.has_value() &&
      !ShouldIgnoreNavigationForPostApplicationMetrics(metadata)) {
    if (metadata.is_navigation_from_omnibox_or_bookmarks) {
      FlushPostSuggestionApplicationSession(
          SessionOutcomeTrigger::kNavigationFromBrowserContext,
          metadata.navigation_start_time);
    } else if (metadata.is_back_navigation) {
      FlushPostSuggestionApplicationSession(
          SessionOutcomeTrigger::kNavigationBack,
          metadata.navigation_start_time);
    } else {
      FlushPostSuggestionApplicationSession(
          SessionOutcomeTrigger::kNavigationFromPageContext,
          metadata.navigation_start_time);
    }
  }

  // If this navigation is applying the suggestion, start the application
  // session.
  if (metadata.applied_suggestion.has_value()) {
    current_suggestion_application_session_ = SuggestionApplicationSession{
        .suggestion = metadata.applied_suggestion.value(),
        .is_error_page = metadata.is_error_page_navigation,
        .suggestion_accepted_time = metadata.navigation_start_time,
        .retention_snapshot =
            last_accepted_suggestion_retention_snapshot_.value_or(
                RetentionStateSnapshot()),
    };
  }
  last_accepted_suggestion_retention_snapshot_.reset();

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
  current_ui_session_ = SuggestionUiSession{
      .suggestion = suggestion,
      .user_decision = SuggestionUserDecision::kIgnored,
      .suggestion_shown_time = base::TimeTicks::Now(),
      .retention_snapshot = retention_snapshot,
      .triggering_navigation_finish_time =
          current_navigation_.navigation_finish_time,
      .ukm_source_id = current_navigation_.ukm_source_id,
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
      last_accepted_suggestion_retention_snapshot_ =
          current_ui_session_->retention_snapshot;
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

void MultistepFilterMetricsTracker::
    OnSuggestionApplicationAnnotationExtractionFinished(
        bool was_applied_successfully) {
  if (!current_suggestion_application_session_.has_value()) {
    return;
  }
  if (was_applied_successfully &&
      !current_suggestion_application_session_->is_error_page) {
    if (current_post_suggestion_application_session_.has_value()) {
      FlushPostSuggestionApplicationSession(
          SessionOutcomeTrigger::kSessionOverride, base::TimeTicks::Now());
    }
    current_post_suggestion_application_session_ =
        PostSuggestionApplicationSession{
            .post_suggestion_window_start_time =
                current_navigation_.navigation_finish_time,
        };
  }
  FlushSuggestionApplicationSession(was_applied_successfully);
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
  current_ui_session_ = std::nullopt;
}

void MultistepFilterMetricsTracker::FlushSuggestionApplicationSession(
    bool was_applied_successfully) {
  CHECK(current_suggestion_application_session_.has_value());
  bool is_success = was_applied_successfully &&
                    !current_suggestion_application_session_->is_error_page;
  LogApplicationOutcome(*current_suggestion_application_session_,
                        current_navigation_, is_success);
  current_suggestion_application_session_ = std::nullopt;
}

void MultistepFilterMetricsTracker::FlushPostSuggestionApplicationSession(
    SessionOutcomeTrigger trigger,
    base::TimeTicks event_time) {
  CHECK(current_post_suggestion_application_session_.has_value());
  MultistepFilterPostSuggestionApplicationUserEngagement user_engagement_action;

  const base::TimeDelta time_since_suggestion_application_finish =
      GetClampedDifference(event_time,
                           current_post_suggestion_application_session_
                               ->post_suggestion_window_start_time);
  const bool within_window =
      time_since_suggestion_application_finish <
      kMultistepFilterPostApplicationSessionDuration.Get();
  using enum MultistepFilterPostSuggestionApplicationUserEngagement;
  switch (trigger) {
    case SessionOutcomeTrigger::kTabClosed:
      user_engagement_action = within_window
                                   ? kAbandonedWithinSessionWindowTabClosed
                                   : kAbandonedAfterSessionWindowTabClosed;
      break;
    case SessionOutcomeTrigger::kNavigationFromBrowserContext:
      user_engagement_action =
          within_window ? kAbandonedWithinSessionWindowOmniboxOrBookmark
                        : kAbandonedAfterSessionWindowOmniboxOrBookmark;
      break;
    case SessionOutcomeTrigger::kNavigationBack:
      user_engagement_action = within_window
                                   ? kAbandonedWithinSessionWindowBackNavigation
                                   : kAbandonedAfterSessionWindowBackNavigation;
      break;
    case SessionOutcomeTrigger::kSessionOverride:
      // Overridden by a new suggestion before any navigation.
      user_engagement_action =
          within_window ? kAbandonedWithinSessionWindowSessionOverride
                        : kAbandonedAfterSessionWindowSessionOverride;
      break;
    case SessionOutcomeTrigger::kNavigationFromPageContext:
      user_engagement_action =
          within_window ? kEngagedWithFurtherNavigationWithinSessionWindow
                        : kEngagedWithFurtherNavigationAfterSessionWindow;
      break;
  }
  base::UmaHistogramEnumeration(
      kMultistepFilterPostSuggestionApplicationUserEngagementHistogram,
      user_engagement_action);
  current_post_suggestion_application_session_ = std::nullopt;
}

}  // namespace multistep_filter
