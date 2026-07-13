// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_TRACKER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_TRACKER_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace multistep_filter {

struct FilterNavigationMetadata;

// A tab-scoped tracker responsible for calculating, maintaining, and flushing
// holistic UMA metrics for the Multistep Filter feature.
class MultistepFilterMetricsTracker {
 public:
  MultistepFilterMetricsTracker();
  MultistepFilterMetricsTracker(const MultistepFilterMetricsTracker&) = delete;
  MultistepFilterMetricsTracker& operator=(
      const MultistepFilterMetricsTracker&) = delete;
  ~MultistepFilterMetricsTracker();

  // --- 1. NAVIGATION FINISH ---
  // Triggered when ANY navigation in the tab completes (or fails).
  void OnNavigationFinished(const FilterNavigationMetadata& metadata);

  // --- 2. SUGGESTION IMPRESSION LIFE-CYCLE ---
  // Triggered when the suggestion UI cue is shown to the user (initial bubble
  // or page action icon). Starts the impression tracking session.
  void OnSuggestionShown(const UrlFilterSuggestion& suggestion);

  // --- 2.1. SUGGESTION REOPEN ---
  // Triggered once if the user re-opens the cue from the Omnibox.
  void OnSuggestionReopened();

  // --- 2.2. SUGGESTION USER INTERACTION ---
  // Triggered when the user interacts with the suggestion UI or a
  // navigation/tab close discards the suggestion.
  void OnSuggestionUserInteraction(SuggestionUserDecision decision);

  // --- 3. SUGGESTION APPLICATION LIFE-CYCLE ---
  // Triggered when the suggestion application extraction is finished.
  // `was_applied_successfully` is true if the navigation completed successfully
  // and the extracted annotations matched the filters the user decided to apply
  // before the navigation.
  void OnSuggestionApplicationAnnotationExtractionFinished(
      bool was_applied_successfully);

 private:
  // Internal helper to calculate and flush UMA for pending suggestion UI
  // sessions.
  void FlushSuggestionUiSession(SuggestionUserDecision final_decision);

  // Internal helper to calculate and flush UMA for pending suggestion
  // application sessions.
  void FlushSuggestionApplicationSession(bool was_applied_successfully);

  struct NavigationSession {
    base::TimeTicks navigation_start_time;
    base::TimeTicks navigation_finish_time;
    bool is_back_navigation = false;
  } current_navigation_;

  // Tracks the UI lifecycle of a multistep filter suggestion.
  //
  // A session begins when the suggestion is first shown to the user.
  // It tracks interactive state transitions (such as collapsing and reopening),
  // timestamps for latency calculations, and the user's final decision
  // (accepted, dismissed, or ignored).
  //
  // The session is flushed and reset when the suggestion is cleared
  // (e.g., due to a new navigation).
  struct SuggestionUiSession {
    UrlFilterSuggestion suggestion;
    bool reopened_cue_shown = false;
    SuggestionUserDecision user_decision = SuggestionUserDecision::kIgnored;
    base::TimeTicks suggestion_shown_time;
    base::TimeTicks suggestion_accepted_time;
  };

  // The current UI session, if any.
  std::optional<SuggestionUiSession> current_ui_session_;

  // Tracks the lifecycle of a suggestion application.
  //
  // A session starts when the user accepts a suggestion, triggering a new
  // navigation. It tracks the navigation outcome (e.g., if it hit an error
  // page) and measures the latency between suggestion acceptance and when the
  // navigation finishes (when the suggestion is applied). This latency is only
  // recorded if the application is subsequently verified by successful
  // annotation extraction on the navigated URL.
  struct SuggestionApplicationSession {
    UrlFilterSuggestion suggestion;
    bool is_error_page = false;
    // TODO(crbug.com/531717350): Populate and use this field to measure
    // suggestion application latency.
    base::TimeTicks suggestion_accepted_time;
  };
  // The current suggestion application session, if any.
  std::optional<SuggestionApplicationSession>
      current_suggestion_application_session_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_TRACKER_H_
