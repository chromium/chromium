// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_FILTER_ACCEPTANCE_METRICS_LOGGER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_FILTER_ACCEPTANCE_METRICS_LOGGER_H_

#include <optional>
#include <string>
#include <string_view>

#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"

namespace multistep_filter {

// An RAII helper class to track and record UMA acceptance metrics for a filter
// suggestion. Accumulates user decisions across UI surfaces (Initial Cue and
// Reopened Cue) and flushes exactly one histogram sample per engaged surface
// upon destruction.
class FilterAcceptanceMetricsLogger {
 public:
  explicit FilterAcceptanceMetricsLogger(std::string_view task_type);
  FilterAcceptanceMetricsLogger(const FilterAcceptanceMetricsLogger&) = delete;
  FilterAcceptanceMetricsLogger& operator=(
      const FilterAcceptanceMetricsLogger&) = delete;
  ~FilterAcceptanceMetricsLogger();

  // Records that a UI surface was shown to the user. Initializes its default
  // decision to kIgnored if not already set.
  void RecordInitialCueShown();
  void RecordReopenedCueShown();

  // Records an explicit user decision on the initial cue surface and sets the
  // overall decision.
  void RecordInitialCueAndOverallDecision(SuggestionUserDecision decision);

  // Records an explicit user decision on the reopened cue surface and sets
  // the overall decision.
  void RecordReopenedCueAndOverallDecision(SuggestionUserDecision decision);

 private:
  std::string task_type_;
  std::optional<SuggestionUserDecision> last_decision_from_initial_cue_;
  std::optional<SuggestionUserDecision> last_decision_from_reopened_cue_;
  std::optional<SuggestionUserDecision> last_decision_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_FILTER_ACCEPTANCE_METRICS_LOGGER_H_
