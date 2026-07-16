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
        navigation_session.navigation_finish_time -
            application_session.suggestion_accepted_time);
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
    const MultistepFilterMetricsTracker::SuggestionUiSession& ui_session,
    const MultistepFilterMetricsTracker::NavigationSession& nav_session) {
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

  CHECK(!nav_session.navigation_finish_time.is_null());
  base::UmaHistogramTimes(
      kMultistepFilterTimeNavigationToSuggestionShownHistogram,
      ui_session.suggestion_shown_time - nav_session.navigation_finish_time);

  base::TimeDelta suggestion_age =
      base::Time::Now() - ui_session.suggestion.extraction_timestamp;
  int suggestion_age_minutes = suggestion_age.InMinutes();
  int max_age_bucket = kMultistepFilterSessionDuration.Get().InMinutes() + 1;
  base::UmaHistogramExactLinear(kMultistepFilterSuggestionAgeShownHistogram,
                                suggestion_age_minutes, max_age_bucket);
  if (GetEtldPlusOneForHost(
          base::UTF16ToUTF8(ui_session.suggestion.source_host)) ==
      GetEtldPlusOneForHost(ui_session.suggestion.triggering_host)) {
    base::UmaHistogramExactLinear(
        kMultistepFilterSuggestionAgeShownOnSameDomainHistogram,
        suggestion_age_minutes, max_age_bucket);
  }
}

void LogSuggestionAcceptanceLatencyAndAgeMetrics(
    const MultistepFilterMetricsTracker::SuggestionUiSession& ui_session,
    const MultistepFilterMetricsTracker::NavigationSession& nav_session) {
  CHECK(!nav_session.navigation_finish_time.is_null());
  base::UmaHistogramMediumTimes(
      kMultistepFilterTimeNavigationToSuggestionAcceptedHistogram,
      ui_session.suggestion_accepted_time - nav_session.navigation_finish_time);

  base::UmaHistogramMediumTimes(
      kMultistepFilterTimeSuggestionShownToAcceptedHistogram,
      ui_session.suggestion_accepted_time - ui_session.suggestion_shown_time);

  base::TimeDelta suggestion_age =
      base::Time::Now() - ui_session.suggestion.extraction_timestamp;
  int suggestion_age_minutes = suggestion_age.InMinutes();
  int max_age_bucket = kMultistepFilterSessionDuration.Get().InMinutes() + 1;
  base::UmaHistogramExactLinear(kMultistepFilterSuggestionAgeAcceptedHistogram,
                                suggestion_age_minutes, max_age_bucket);
  if (GetEtldPlusOneForHost(
          base::UTF16ToUTF8(ui_session.suggestion.source_host)) ==
      GetEtldPlusOneForHost(ui_session.suggestion.triggering_host)) {
    base::UmaHistogramExactLinear(
        kMultistepFilterSuggestionAgeAcceptedOnSameDomainHistogram,
        suggestion_age_minutes, max_age_bucket);
  }
}

MultistepFilterPostSuggestionApplicationFirstNavigation
CalculatePostSuggestionApplicationFirstNavigationAction(
    const MultistepFilterMetricsTracker::PostSuggestionApplicationSession&
        session,
    const FilterNavigationMetadata& metadata) {
  if (metadata.is_back_navigation) {
    // Use the navigation start time (when the user initiated the back action)
    // rather than the current time (when the navigation finished) to measure
    // the user's actual dwell time on the page, excluding loading latency.
    base::TimeDelta time_since_landing =
        metadata.navigation_start_time -
        session.post_suggestion_window_start_time;
    bool within_window = time_since_landing <
                         kMultistepFilterPostApplicationSessionDuration.Get();
    return within_window
               ? MultistepFilterPostSuggestionApplicationFirstNavigation::
                     kBackNavigationWithinSessionWindow
               : MultistepFilterPostSuggestionApplicationFirstNavigation::
                     kBackNavigationAfterSessionWindow;
  }
  return MultistepFilterPostSuggestionApplicationFirstNavigation::
      kForwardOrOtherNavigation;
}

// Helper to determine if a navigation should be ignored for post-application
// metrics.
bool ShouldIgnoreNavigationForPostApplicationMetrics(
    const FilterNavigationMetadata& metadata) {
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
    FlushPostSuggestionApplicationSession();
  }
}

void MultistepFilterMetricsTracker::OnNavigationFinished(
    const FilterNavigationMetadata& metadata) {
  CHECK(!metadata.navigation_start_time.is_null());
  CHECK(!metadata.navigation_finish_time.is_null());
  current_navigation_ = NavigationSession();
  current_navigation_.navigation_finish_time = metadata.navigation_finish_time;

  // If a new navigation finished while we were still waiting for extraction
  // of a previously applied suggestion, that application session is considered
  // failed.
  if (current_suggestion_application_session_.has_value()) {
    FlushSuggestionApplicationSession(/*was_applied_successfully=*/false);
  }

  // If a post-acceptance session is active, track this navigation.
  if (current_post_suggestion_application_session_.has_value()) {
    TrackPostSuggestionApplicationNavigation(metadata);
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
  };
  LogSuggestionUiShown(*current_ui_session_, current_navigation_);
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
      FlushPostSuggestionApplicationSession();
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
    LogSuggestionAcceptanceLatencyAndAgeMetrics(*current_ui_session_,
                                                current_navigation_);
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

void MultistepFilterMetricsTracker::TrackPostSuggestionApplicationNavigation(
    const FilterNavigationMetadata& metadata) {
  CHECK(current_post_suggestion_application_session_.has_value());
  if (ShouldIgnoreNavigationForPostApplicationMetrics(metadata) ||
      current_post_suggestion_application_session_
          ->has_logged_first_navigation) {
    return;
  }
  base::UmaHistogramEnumeration(
      kMultistepFilterPostSuggestionApplicationFirstNavigationHistogram,
      CalculatePostSuggestionApplicationFirstNavigationAction(
          *current_post_suggestion_application_session_, metadata));
  current_post_suggestion_application_session_->has_logged_first_navigation =
      true;
}

void MultistepFilterMetricsTracker::FlushPostSuggestionApplicationSession() {
  CHECK(current_post_suggestion_application_session_.has_value());
  MultistepFilterPostSuggestionApplicationTabClose close_action;

  if (current_post_suggestion_application_session_
          ->has_logged_first_navigation) {
    close_action = MultistepFilterPostSuggestionApplicationTabClose::
        kTabClosedWithFurtherNavigation;
  } else {
    base::TimeDelta time_since_suggestion_application_finish =
        base::TimeTicks::Now() - current_post_suggestion_application_session_
                                     ->post_suggestion_window_start_time;
    bool within_window = time_since_suggestion_application_finish <
                         kMultistepFilterPostApplicationSessionDuration.Get();
    close_action = within_window
                       ? MultistepFilterPostSuggestionApplicationTabClose::
                             kTabClosedWithinSessionWindow
                       : MultistepFilterPostSuggestionApplicationTabClose::
                             kTabClosedAfterSessionWindow;
  }
  base::UmaHistogramEnumeration(
      kMultistepFilterPostSuggestionApplicationTabCloseHistogram, close_action);
  current_post_suggestion_application_session_ = std::nullopt;
}

}  // namespace multistep_filter
