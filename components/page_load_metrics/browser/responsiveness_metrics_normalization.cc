// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/responsiveness_metrics_normalization.h"

namespace page_load_metrics {
// Budget for each type of user interaction, including keyboard, click or tap,
// drag.
constexpr base::TimeDelta kBudgetForKeyboard = base::Milliseconds(50);
constexpr base::TimeDelta kBudgetForClickOrTap = base::Milliseconds(100);
constexpr base::TimeDelta kBudgetForDrag = base::Milliseconds(100);

namespace {

base::TimeDelta LatencyOverBudget(
    const mojom::UserInteractionLatencyPtr& user_interaction) {
  base::TimeDelta latency = user_interaction->interaction_latency;
  switch (user_interaction->interaction_type) {
    case mojom::UserInteractionType::kKeyboard:
      return std::max(latency - kBudgetForKeyboard, base::Milliseconds(0));
    case mojom::UserInteractionType::kTapOrClick:
      return std::max(latency - kBudgetForClickOrTap, base::Milliseconds(0));
    case mojom::UserInteractionType::kDrag:
      return std::max(latency - kBudgetForDrag, base::Milliseconds(0));
  }
  NOTREACHED();
  return latency;
}

}  // namespace

NormalizedInteractionLatencies::NormalizedInteractionLatencies() = default;
NormalizedInteractionLatencies::~NormalizedInteractionLatencies() = default;

NormalizedResponsivenessMetrics::NormalizedResponsivenessMetrics() = default;
NormalizedResponsivenessMetrics::~NormalizedResponsivenessMetrics() = default;

ResponsivenessMetricsNormalization::ResponsivenessMetricsNormalization() =
    default;
ResponsivenessMetricsNormalization::~ResponsivenessMetricsNormalization() =
    default;

// static
base::TimeDelta ResponsivenessMetricsNormalization::ApproximateHighPercentile(
    uint64_t num_interactions,
    std::priority_queue<base::TimeDelta,
                        std::vector<base::TimeDelta>,
                        std::greater<>> worst_ten_latencies) {
  DCHECK(num_interactions);
  int index = std::max(0, static_cast<int>(worst_ten_latencies.size()) - 1 -
                              static_cast<int>(num_interactions /
                                               kHighPercentileUpdateFrequency));
  for (; index > 0; index--) {
    worst_ten_latencies.pop();
  }

  return worst_ten_latencies.top();
}

void ResponsivenessMetricsNormalization::AddNewUserInteractionLatencies(
    uint64_t num_new_interactions,
    const mojom::UserInteractionLatencies& max_event_durations) {
  uint64_t last_num_user_interactions =
      normalized_responsiveness_metrics_.num_user_interactions;
  normalized_responsiveness_metrics_.num_user_interactions +=
      num_new_interactions;
  DCHECK(max_event_durations.is_user_interaction_latencies() ||
         max_event_durations.is_worst_interaction_latency());
  // Normalize max event durations.
  NormalizeUserInteractionLatencies(
      max_event_durations,
      normalized_responsiveness_metrics_.normalized_max_event_durations,
      last_num_user_interactions,
      normalized_responsiveness_metrics_.num_user_interactions);
}

void ResponsivenessMetricsNormalization::NormalizeUserInteractionLatencies(
    const mojom::UserInteractionLatencies& user_interaction_latencies,
    NormalizedInteractionLatencies& normalized_event_durations,
    uint64_t last_num_user_interactions,
    uint64_t current_num_user_interactions) {
  DCHECK(user_interaction_latencies.is_user_interaction_latencies());
  for (const mojom::UserInteractionLatencyPtr& user_interaction :
       user_interaction_latencies.get_user_interaction_latencies()) {
    normalized_event_durations.worst_latency =
        std::max(normalized_event_durations.worst_latency,
                 user_interaction->interaction_latency);
    base::TimeDelta latency_over_budget = LatencyOverBudget(user_interaction);
    normalized_event_durations.worst_latency_over_budget =
        std::max(normalized_event_durations.worst_latency_over_budget,
                 latency_over_budget);
    normalized_event_durations.sum_of_latency_over_budget +=
        latency_over_budget;
    normalized_event_durations.worst_ten_latencies.push(
        user_interaction->interaction_latency);
    if (normalized_event_durations.worst_ten_latencies.size() == 11) {
      normalized_event_durations.worst_ten_latencies.pop();
    }
    normalized_event_durations.worst_ten_latencies_over_budget.push(
        latency_over_budget);
    if (normalized_event_durations.worst_ten_latencies_over_budget.size() ==
        11) {
      normalized_event_durations.worst_ten_latencies_over_budget.pop();
    }
    if (latency_over_budget >=
        normalized_event_durations.high_percentile_latency_over_budget) {
      normalized_event_durations.high_percentile_latency_over_budget =
          latency_over_budget;
    }
  }
}

}  // namespace page_load_metrics
