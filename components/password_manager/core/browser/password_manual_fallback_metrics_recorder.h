// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_METRICS_RECORDER_H_

#include "base/time/time.h"

namespace password_manager {

// Encapsulates logic for logging password manual fallback related metrics.
// The lifetime of this class is managed by `PasswordAutofillManager`, which
// destroys the metrics recorder on navigation. The metrics recorder collects
// some of the metrics until destroyed, at which point those metrics are
// emitted. Context menu related tests are in
// `autofill_context_menu_manager_browsertest.cc`. Suggestion related tests are
// in `password_manual_fallback_flow_unittest.cc`.
class PasswordManualFallbackMetricsRecorder {
 public:
  PasswordManualFallbackMetricsRecorder();
  PasswordManualFallbackMetricsRecorder(
      const PasswordManualFallbackMetricsRecorder&) = delete;
  PasswordManualFallbackMetricsRecorder& operator=(
      const PasswordManualFallbackMetricsRecorder&) = delete;
  // Emits metrics before destruction.
  ~PasswordManualFallbackMetricsRecorder();

  // Assigns the current time to `latency_duration_start_`, which is then used
  // inside `RecordDataFetchingLatency()` to calculate how much time has passed
  // between the start of the fetch and the end of the fetch.
  void DataFetchingStarted();

  // Records "PasswordManager.ManualFallback.ShowSuggestions.Latency" metric.
  void RecordDataFetchingLatency() const;

  // Called when a suggestion is shown.
  void OnDidShowSuggestions(bool classified_as_target_filling_password);

  // Called when a suggestion is filled.
  void OnDidFillSuggestion(bool classified_as_target_filling_password);

  // Called when context menu was opened on a qualifying field.
  // `classified_as_target_filling_password` is true if the triggering field is
  // classified as a username or a password field.
  void ContextMenuEntryShown(bool classified_as_target_filling_password);

  // Called when the fallback entry was accepted (not just hovered).
  // `classified_as_target_filling_password` is true if the triggering field is
  // classified as a username or a password field.
  void ContextMenuEntryAccepted(bool classified_as_target_filling_password);

 private:
  enum class ContextMenuEntryState { kNotShown = 0, kShown = 1, kAccepted = 2 };
  enum class SuggestionState { kNotShown = 0, kShown = 1, kFilled = 2 };

  // If the context menu was used according to the `state`, emits whether the
  // entry was accepted or not into the explicit triggering metric of the given
  // `bucket` (classified or not classified as target filling password).
  // `record_to_total_not_classified_as_target_filling_bucket` decides whether
  // to record a "Total" variant of the metric. Only
  // "NotClassifiedAsTargetFilling" metrics have a "Total" variant.
  // Classified password fields don't have a "Total" variant, because only
  // passwords record the "ClassifiedAsTargetFilling" metric variant. I.e. The
  // other filling products (addresses and credit cards), do not record the
  // "ClassifiedAsTargetFilling" metric variant. This is because classified
  // address fields fall into the autocomplete recognized/unrecognized metrics,
  // while classified credit card fields do not trigger any different behaviour
  // and work the same as regular left click (therefore, no specific metric is
  // emitted for them). On the other hand, password manual fallback always
  // triggers a different behavior on right-click (suggestions have a search
  // bar).
  void EmitExplicitlyTriggeredMetric(
      ContextMenuEntryState state,
      std::string_view bucket,
      bool record_to_total_not_classified_as_target_filling_bucket);

  // If suggestions were shown, emits whether the entry was accepted or not into
  // the explicit triggering metric of the given `bucket` (classified or not
  // classified as target filling password).
  void EmitFillAfterSuggestionMetric(SuggestionState suggestion_state,
                                     std::string_view bucket);

  base::Time latency_duration_start_;

  // Tracks if the manual fallback context menu entry was shown or accepted.
  ContextMenuEntryState not_classified_as_target_filling_context_menu_state_ =
      ContextMenuEntryState::kNotShown;
  ContextMenuEntryState classified_as_target_filling_context_menu_state_ =
      ContextMenuEntryState::kNotShown;

  // Tracks if suggestions were shown/filled.
  SuggestionState not_classified_as_target_filling_suggestion_state_ =
      SuggestionState::kNotShown;
  SuggestionState classified_as_target_filling_suggestion_state_ =
      SuggestionState::kNotShown;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_METRICS_RECORDER_H_
