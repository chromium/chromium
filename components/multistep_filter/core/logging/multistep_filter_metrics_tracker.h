// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_TRACKER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_TRACKER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/prefs/retention_state_snapshot.h"

namespace multistep_filter {

struct FilterNavigationMetadata;

// A tab-scoped tracker responsible for calculating, maintaining, and flushing
// holistic UMA metrics for the Multistep Filter feature.
class MultistepFilterMetricsTracker {
 public:
  struct NavigationSession {
    base::TimeTicks navigation_finish_time;
    bool is_back_navigation = false;
  };

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
    RetentionStateSnapshot retention_snapshot;
    bool is_preserved_same_page = false;
  };

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
    base::TimeTicks suggestion_accepted_time;
    RetentionStateSnapshot retention_snapshot;
  };

  // Tracks the user's behavior after accepting a suggestion.
  //
  // A session starts when the navigation triggered by accepting a suggestion
  // finishes (either successfully or with an error). It tracks subsequent
  // navigations and tab closure to log post-application behavior within a
  // session window.
  struct PostSuggestionApplicationSession {
    // The time when the navigation triggered by the accepted suggestion
    // finished. This marks the start of the post-application session window.
    base::TimeTicks post_suggestion_window_start_time;
    // Whether the first navigation after the suggestion application has been
    // logged. If true, any subsequent navigations within the session window
    // will be ignored.
    bool has_logged_first_navigation = false;
  };

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
  void OnSuggestionShown(const UrlFilterSuggestion& suggestion,
                         const RetentionStateSnapshot& retention_snapshot);

  // --- 2.1. SUGGESTION REOPEN ---
  // Triggered once if the user re-opens the cue from the Omnibox.
  void OnSuggestionReopened();

  // --- 2.2. SUGGESTION USER INTERACTION ---
  // Triggered when the user interacts with the suggestion UI or a
  // navigation/tab close discards the suggestion.
  void OnSuggestionUserInteraction(SuggestionUserDecision decision);

  // --- 2.3. PRESERVED SUGGESTION CLEARED ---
  // Triggered when a preserved suggestion for same page navigation is cleared
  // (e.g. because new suggestion generation failed).
  void OnPreservedSuggestionCleared();

  // --- 3. SUGGESTION APPLICATION LIFE-CYCLE ---
  // Triggered when the suggestion application extraction is finished.
  // `was_applied_successfully` is true if the navigation completed successfully
  // and the extracted annotations matched the filters the user decided to apply
  // before the navigation.
  // If successful, this starts a post-application session to track behavior
  // within a session window (controlled by
  // `kMultistepFilterPostApplicationSessionDuration`).
  void OnSuggestionApplicationAnnotationExtractionFinished(
      bool was_applied_successfully);

 private:
  // Internal helper to calculate and flush UMA for pending suggestion UI
  // sessions.
  void FlushSuggestionUiSession(SuggestionUserDecision final_decision);

  // Internal helper to calculate and flush UMA for pending suggestion
  // application sessions.
  void FlushSuggestionApplicationSession(bool was_applied_successfully);

  // Internal helper to track navigation after a suggestion was successfully
  // applied. Only tracks the first non-ignored navigation within the session
  // window (controlled by `kMultistepFilterPostApplicationSessionDuration`).
  void TrackPostSuggestionApplicationNavigation(
      const FilterNavigationMetadata& metadata);

  // Internal helper to flush UMA for the post-suggestion application session
  // (e.g. on tab close or when a new session starts). Only applicable for
  // successful suggestion applications. The window duration is controlled by
  // `kMultistepFilterPostApplicationSessionDuration`.
  void FlushPostSuggestionApplicationSession();

  // The current navigation session.
  NavigationSession current_navigation_;
  // The current UI session, if any.
  std::optional<SuggestionUiSession> current_ui_session_;
  // The current suggestion application session, if any.
  std::optional<SuggestionApplicationSession>
      current_suggestion_application_session_;
  // The retention state snapshot of the last accepted suggestion. It is
  // set when a suggestion is accepted, and cleared when the next navigation
  // finishes (either consumed by the application session or discarded).
  std::optional<RetentionStateSnapshot>
      last_accepted_suggestion_retention_snapshot_;
  // The current post-suggestion application session, if any. This is only
  // created for successful suggestion applications to track behavior within a
  // session window (controlled by
  // `kMultistepFilterPostApplicationSessionDuration`).
  std::optional<PostSuggestionApplicationSession>
      current_post_suggestion_application_session_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_TRACKER_H_
