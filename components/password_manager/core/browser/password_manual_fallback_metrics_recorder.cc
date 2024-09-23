// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace password_manager {

namespace {
constexpr char kShowSuggestionLatency[] =
    "PasswordManager.ManualFallback.ShowSuggestions.Latency";
}  // namespace

PasswordManualFallbackMetricsRecorder::PasswordManualFallbackMetricsRecorder() =
    default;

PasswordManualFallbackMetricsRecorder::
    ~PasswordManualFallbackMetricsRecorder() {
  EmitExplicitlyTriggeredMetric(
      classified_as_target_filling_context_menu_state_,
      "ClassifiedAsTargetFilling",
      /*record_to_total_not_classified_as_target_filling_bucket=*/false);
  EmitExplicitlyTriggeredMetric(
      not_classified_as_target_filling_context_menu_state_,
      "NotClassifiedAsTargetFilling",
      /*record_to_total_not_classified_as_target_filling_bucket=*/true);
  EmitFillAfterSuggestionMetric(classified_as_target_filling_suggestion_state_,
                                "ClassifiedAsTargetFilling");
  EmitFillAfterSuggestionMetric(
      not_classified_as_target_filling_suggestion_state_,
      "NotClassifiedAsTargetFilling");
}

void PasswordManualFallbackMetricsRecorder::DataFetchingStarted() {
  latency_duration_start_ = base::Time::Now();
}

void PasswordManualFallbackMetricsRecorder::RecordDataFetchingLatency() const {
  base::TimeDelta duration = base::Time::Now() - latency_duration_start_;

  base::UmaHistogramTimes(kShowSuggestionLatency, duration);
}

void PasswordManualFallbackMetricsRecorder::OnDidShowSuggestions(
    bool classified_as_target_filling_password) {
  SuggestionState& state =
      classified_as_target_filling_password
          ? classified_as_target_filling_suggestion_state_
          : not_classified_as_target_filling_suggestion_state_;
  if (state != SuggestionState::kFilled) {
    state = SuggestionState::kShown;
  }
}

void PasswordManualFallbackMetricsRecorder::OnDidFillSuggestion(
    bool classified_as_target_filling_password) {
  SuggestionState& state =
      classified_as_target_filling_password
          ? classified_as_target_filling_suggestion_state_
          : not_classified_as_target_filling_suggestion_state_;
  CHECK_NE(state, SuggestionState::kNotShown);
  state = SuggestionState::kFilled;
}

void PasswordManualFallbackMetricsRecorder::ContextMenuEntryShown(
    bool classified_as_target_filling_password) {
  ContextMenuEntryState& state =
      classified_as_target_filling_password
          ? classified_as_target_filling_context_menu_state_
          : not_classified_as_target_filling_context_menu_state_;
  if (state != ContextMenuEntryState::kAccepted) {
    state = ContextMenuEntryState::kShown;
  }
}

void PasswordManualFallbackMetricsRecorder::ContextMenuEntryAccepted(
    bool classified_as_target_filling_password) {
  ContextMenuEntryState& state =
      classified_as_target_filling_password
          ? classified_as_target_filling_context_menu_state_
          : not_classified_as_target_filling_context_menu_state_;
  CHECK_NE(state, ContextMenuEntryState::kNotShown);
  state = ContextMenuEntryState::kAccepted;
}

void PasswordManualFallbackMetricsRecorder::EmitExplicitlyTriggeredMetric(
    ContextMenuEntryState context_menu_state,
    std::string_view bucket,
    bool record_to_total_not_classified_as_target_filling_bucket) {
  if (context_menu_state == ContextMenuEntryState::kNotShown) {
    return;
  }

  auto metric_name = [](std::string_view token1, std::string_view token2) {
    return base::StrCat(
        {"Autofill.ManualFallback.ExplicitlyTriggered.", token1, ".", token2});
  };
  const bool was_accepted =
      context_menu_state == ContextMenuEntryState::kAccepted;
  base::UmaHistogramBoolean(metric_name(bucket, "Password"), was_accepted);
  if (record_to_total_not_classified_as_target_filling_bucket) {
    base::UmaHistogramBoolean(metric_name(bucket, "Total"), was_accepted);
  }
}

void PasswordManualFallbackMetricsRecorder::EmitFillAfterSuggestionMetric(
    SuggestionState suggestion_state,
    std::string_view bucket) {
  if (suggestion_state == SuggestionState::kNotShown) {
    return;
  }
  base::UmaHistogramBoolean(base::StrCat({"Autofill.Funnel.", bucket,
                                          ".FillAfterSuggestion.Password"}),
                            suggestion_state == SuggestionState::kFilled);
}

}  // namespace password_manager
