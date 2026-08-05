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
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "components/multistep_filter/core/prefs/retention_state_snapshot.h"
#include "components/multistep_filter/core/verification/suggestion_application_result.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace multistep_filter {

struct FilterNavigationMetadata;

// A tab-scoped tracker responsible for calculating, maintaining, and flushing
// holistic UMA metrics for the Multistep Filter feature.
class MultistepFilterMetricsTracker {
 public:
  struct NavigationSession {
    // Do not use navigation_finish_time for latency calculations as this gets
    // updated for all navigations. Use
    // SuggestionUiSession::triggering_navigation_finish_time instead.
    base::TimeTicks navigation_finish_time;
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
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
    // Time when the navigation finishes that triggered the suggestion (i.e.
    // the navigation preceding the suggestion).
    base::TimeTicks triggering_navigation_finish_time;
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
    // A transient, randomly generated identifier used to correlate the UI
    // session event (MultistepFilter.UiSession) with the subsequent landing
    // page application session event (MultistepFilter.ApplicationSession) in
    // UKM. Regenerated for each suggestion impression.
    int64_t session_id = 0;
    UrlFilterSuggestion suggestion;
    RetentionStateSnapshot retention_snapshot;
    base::TimeTicks suggestion_shown_time;
    // Time delta since the filter_annotation which comprises this suggestion
    // was extracted to the time when the suggestion was shown to the user.
    base::TimeDelta extraction_to_suggestion_shown_time_delta;
    base::TimeDelta navigation_to_suggestion_shown_latency;
    // True if the suggestion was shown on the same eTLD+1 as the page from
    // which it was extracted.
    bool is_same_domain = false;
    bool reopened_cue_shown = false;
    SuggestionUserDecision user_decision = SuggestionUserDecision::kIgnored;
    base::TimeTicks suggestion_accepted_time;
    base::TimeDelta navigation_to_suggestion_accepted_time_delta;
    base::TimeDelta suggestion_shown_to_accepted_time_delta;
    // Time delta since the filter_annotation which comprises this suggestion
    // was extracted to the time when the suggestion was accepted by the user.
    base::TimeDelta extraction_to_suggestion_accepted_time_delta;
    bool is_preserved_same_page = false;
  };

  // Holds metrics metadata about the last accepted suggestion (like session ID
  // and retention history) that needs to be carried over across the navigation
  // triggered by the acceptance, until the navigation finishes and the
  // suggestion is applied.
  //
  // Lifecycle:
  // - Created: In `OnSuggestionUserInteraction` when the user accepts a
  //   suggestion.
  // - Reset: In `OnNavigationFinished` when the navigation finishes (either
  //   consumed to start a `SuggestionApplicationSession`, or discarded
  //   if the navigation was not filter-initiated).
  struct PendingSuggestionApplicationMetrics {
    RetentionStateSnapshot retention_snapshot;
    int64_t session_id = 0;
    base::TimeTicks suggestion_accepted_time;
  };

  // Tracks the lifecycle of a suggestion application and subsequent user
  // engagement.
  //
  // A session starts when the navigation triggered by accepting a suggestion
  // finishes. It tracks the application outcome (success/failure) and, if
  // successful, subsequent navigations and tab closure to log post-application
  // behavior within a session window.
  struct SuggestionApplicationSession {
    // The UKM source ID of the navigation that is applying the suggestion (the
    // landing page).
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
    // A transient, randomly generated identifier used to correlate the UI
    // session event (MultistepFilter.UiSession) with the subsequent landing
    // page application session event (MultistepFilter.ApplicationSession) in
    // UKM. Regenerated for each suggestion impression.
    int64_t session_id = 0;
    UrlFilterSuggestion suggestion;
    RetentionStateSnapshot retention_snapshot;
    base::TimeTicks suggestion_accepted_time;
    bool is_error_page = false;
    base::TimeTicks application_navigation_finish_time;
    base::TimeDelta suggestion_accepted_to_applied_latency;
    SuggestionApplicationResult outcome =
        SuggestionApplicationResult::kAbandonedBeforeVerification;
    // True if the suggestion was successfully applied and we are now tracking
    // post-application user engagement.
    bool is_applied = false;
    // True if the application outcome has already been logged.
    bool outcome_logged = false;
  };

  // Internal trigger representing the reason why a suggestion application
  // session is being flushed. This is mapped to the final metric values
  // (which also depend on whether the event occurred within the session
  // window).
  enum class SuggestionApplicationSessionFlushTrigger {
    // The suggestion failed to apply (e.g. extraction failed or error page).
    kApplicationFailure,
    // The user engaged with the page, which is implied by clicking on links or
    // submitting forms (both from the page context). The destination (to the
    // same domain or to a different domain) is not considered. This is recorded
    // even if the navigation failed and resulted in an error page.
    kNavigationFromPageContext,
    // The tab containing the suggestion page was closed.
    kTabClosed,
    // The user navigated by typing in the Omnibox or clicking on a bookmark,
    // which implies starting from a fresh state rather than continuing the
    // active session with a suggestion.
    kNavigationFromBrowserContext,
    // The user navigated back by pressing the browser's back button.
    kNavigationBack,
    // A new suggestion application overrode the active session.
    kSessionOverride,
  };

  // Tracks the lifecycle of a suggestion impression that was ignored or
  // dismissed by the user (i.e. not accepted).
  struct IgnoredImpressionTracker {
    // The suggestion that was shown to the user and ignored or dismissed.
    UrlFilterSuggestion suggestion;
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
  // Triggered when suggestion application finishes (either successfully or with
  // a failure).
  void OnSuggestionApplicationFinished(SuggestionApplicationResult result);

  // Triggered when the filter attributes are extracted from the landing
  // page. This is used to determine whether the user applied filters on the
  // landing page after ignoring a multistep filter suggestion.
  void OnExtractionFinished(const FilterNavigationMetadata& metadata,
                            const std::optional<FilterAnnotation>& annotation);

 private:
  // Internal helper to calculate and flush metrics for pending suggestion UI
  // sessions.
  void FlushSuggestionUiSession(SuggestionUserDecision final_decision);

  // Internal helper to flush metrics for ignored suggestion impressions.
  void FlushIgnoredImpressionSession(
      MultistepFilterUserBehaviorAfterIgnore outcome);

  // Internal helper to flush metrics for the suggestion application and
  // engagement session (e.g. on tab close, new navigation, or extraction
  // finished).
  //
  // `event_time` is used for post-application engagement duration calculations.
  // It must be the timestamp when the user action was *initiated* (e.g.,
  // `metadata.navigation_start_time` for navigations, or
  // `base::TimeTicks::Now()` for tab close). We pass this explicitly to prevent
  // slow-loading destination pages from inflating the calculated dwell time.
  void FlushSuggestionApplicationSession(
      SuggestionApplicationSessionFlushTrigger trigger,
      base::TimeTicks event_time);

  // Decides whether to flush the active application/engagement session when a
  // navigation finishes, based on the navigation metadata and session state.
  void MaybeFlushSessionOnNavigation(const FilterNavigationMetadata& metadata);

  // The current navigation session.
  NavigationSession current_navigation_;
  // The current UI session, if any.
  std::optional<SuggestionUiSession> current_ui_session_;
  // The active suggestion application session, if any.
  //
  // This is created in `OnNavigationFinished` when the navigation triggered by
  // accepting a suggestion finishes. It first tracks the application outcome
  // (verification) and, if successful, remains active to track subsequent user
  // engagement (dwell time, further navigations, tab close) within a session
  // window (controlled by `kMultistepFilterPostApplicationSessionDuration`).
  std::optional<SuggestionApplicationSession> current_application_session_;

  // Holds metrics metadata (session ID and retention snapshot) of the last
  // accepted suggestion.
  //
  // This is set when the user accepts a suggestion, and carried over the
  // navigation triggered by that acceptance. It is consumed to initialize the
  // `current_application_session_` when the navigation finishes, or discarded
  // if the navigation was aborted or not filter-initiated.
  std::optional<PendingSuggestionApplicationMetrics>
      pending_suggestion_application_metrics_;

  // An ignored impression session begins when a suggestion is ignored or
  // dismissed. It tracks the suggestion that was ignored. The session is
  // flushed (logging the post-ignore outcome) when:
  // - The tab is closed.
  // - A navigation occurs to an error page, a different host, or is
  //   filter-initiated.
  // - OnExtractionFinished is called (for manual same-host filtering check).
  // - A new suggestion is ignored (overwriting the active ignored session).
  std::optional<IgnoredImpressionTracker> ignored_impression_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_TRACKER_H_
