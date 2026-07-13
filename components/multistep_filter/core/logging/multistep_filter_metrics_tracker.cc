// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_metrics_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"

namespace multistep_filter {

namespace {

void LogAcceptanceHistogram(std::string_view base_histogram,
                            std::string_view task_type,
                            SuggestionUserDecision decision) {
  base::UmaHistogramEnumeration(std::string(base_histogram), decision);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {base_histogram, kMultistepFilterByTaskHistogramPrefix, task_type}),
      decision);
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
void LogApplicationOutcome(const UrlFilterSuggestion& suggestion,
                           bool is_success) {
  MultistepFilterApplicationOutcome outcome =
      is_success ? MultistepFilterApplicationOutcome::kAllFiltersApplied
                 : MultistepFilterApplicationOutcome::kNotAllFiltersApplied;
  base::UmaHistogramEnumeration(kMultistepFilterApplicationOutcomeHistogram,
                                outcome);
  base::UmaHistogramEnumeration(
      base::StrCat({kMultistepFilterApplicationOutcomeHistogram,
                    kMultistepFilterByTaskHistogramPrefix,
                    suggestion.task_type}),
      outcome);
  if (is_success) {
    size_t count = suggestion.attribute_ui_labels.size();
    base::UmaHistogramCounts100(
        kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, count);
    base::UmaHistogramCounts100(
        base::StrCat(
            {kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram,
             kMultistepFilterByTaskHistogramPrefix, suggestion.task_type}),
        count);
  }

  for (const FilterAttributeUiLabel& suggested_label :
       suggestion.attribute_ui_labels) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {kMultistepFilterApplicationOutcomeHistogram,
             kMultistepFilterByTaskHistogramPrefix, suggestion.task_type,
             kMultistepFilterByFacetHistogramPrefix, suggested_label.key}),
        is_success);
  }
}

void LogFacetsShown(std::string_view task_type, size_t count) {
  if (count == 0) {
    return;
  }
  base::UmaHistogramCounts100(kMultistepFilterNumberOfFacetsShownHistogram,
                              count);
  base::UmaHistogramCounts100(
      base::StrCat({kMultistepFilterNumberOfFacetsShownHistogram,
                    kMultistepFilterByTaskHistogramPrefix, task_type}),
      count);
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
}

void MultistepFilterMetricsTracker::OnNavigationFinished(
    const FilterNavigationMetadata& metadata) {
  // TODO(crbug.com/531717350): Use these fields to track navigation metrics.
  current_navigation_ = NavigationSession();
  current_navigation_.navigation_finish_time = base::TimeTicks::Now();

  // If a new navigation finished while we were still waiting for extraction
  // of a previously applied suggestion, that application session is considered
  // failed.
  if (current_suggestion_application_session_.has_value()) {
    FlushSuggestionApplicationSession(/*was_applied_successfully=*/false);
  }

  // If this navigation is applying the suggestion, start the application
  // session.
  if (metadata.applied_suggestion.has_value()) {
    current_suggestion_application_session_ = SuggestionApplicationSession{
        .suggestion = metadata.applied_suggestion.value(),
        .is_error_page = metadata.is_error_page_navigation,
    };
  }

  // If a UI session has been started but not concluded yet, the finishing
  // of the navigation indicates that the user ignored the UI.
  if (current_ui_session_.has_value()) {
    FlushSuggestionUiSession(SuggestionUserDecision::kIgnored);
  }
}

void MultistepFilterMetricsTracker::OnSuggestionShown(
    const UrlFilterSuggestion& suggestion) {
  current_ui_session_ = SuggestionUiSession{
      .suggestion = suggestion,
      .user_decision = SuggestionUserDecision::kIgnored,
      .suggestion_shown_time = base::TimeTicks::Now(),
  };
  LogFacetsShown(suggestion.task_type, suggestion.attribute_ui_labels.size());
}

void MultistepFilterMetricsTracker::OnSuggestionReopened() {
  CHECK(current_ui_session_.has_value());
  current_ui_session_->reopened_cue_shown = true;
}

void MultistepFilterMetricsTracker::OnSuggestionUserInteraction(
    SuggestionUserDecision decision) {
  CHECK(current_ui_session_.has_value());
  switch (decision) {
    case SuggestionUserDecision::kAccepted:
      current_ui_session_->suggestion_accepted_time = base::TimeTicks::Now();
      break;
    case SuggestionUserDecision::kDismissed:
    case SuggestionUserDecision::kSettingsOpened:
    case SuggestionUserDecision::kIgnored:
      break;
  }
  current_ui_session_->user_decision = decision;
  FlushSuggestionUiSession(decision);
}

void MultistepFilterMetricsTracker::
    OnSuggestionApplicationAnnotationExtractionFinished(
        bool was_applied_successfully) {
  if (!current_suggestion_application_session_.has_value()) {
    return;
  }
  FlushSuggestionApplicationSession(was_applied_successfully);
}

void MultistepFilterMetricsTracker::FlushSuggestionUiSession(
    SuggestionUserDecision final_decision) {
  CHECK(current_ui_session_.has_value());
  const std::string& task_type = current_ui_session_->suggestion.task_type;
  // If the reopened cue was shown, the initial cue was ignored.
  SuggestionUserDecision initial_decision =
      current_ui_session_->reopened_cue_shown ? SuggestionUserDecision::kIgnored
                                              : final_decision;
  LogAcceptanceHistogram(kMultistepFilterAcceptanceInitialCueHistogram,
                         task_type, initial_decision);

  // If the reopened cue was shown, log the final decision to the reopened cue
  // histogram.
  if (current_ui_session_->reopened_cue_shown) {
    LogAcceptanceHistogram(kMultistepFilterAcceptanceReopenedCueHistogram,
                           task_type, final_decision);
  }

  LogAcceptanceHistogram(kMultistepFilterAcceptanceHistogram, task_type,
                         final_decision);

  current_ui_session_ = std::nullopt;
}

void MultistepFilterMetricsTracker::FlushSuggestionApplicationSession(
    bool was_applied_successfully) {
  CHECK(current_suggestion_application_session_.has_value());
  LogApplicationOutcome(current_suggestion_application_session_->suggestion,
                        was_applied_successfully);
  current_suggestion_application_session_ = std::nullopt;
}

}  // namespace multistep_filter
