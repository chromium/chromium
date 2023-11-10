// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/responsiveness_metrics_normalization.h"

namespace page_load_metrics {

ResponsivenessMetricsNormalization::ResponsivenessMetricsNormalization() =
    default;
ResponsivenessMetricsNormalization::~ResponsivenessMetricsNormalization() =
    default;

absl::optional<base::TimeDelta>
ResponsivenessMetricsNormalization::ApproximateHighPercentile() const {
  absl::optional<base::TimeDelta> approximate_high_percentile;
  if (worst_ten_latencies_.size()) {
    int index = static_cast<int>(num_user_interactions_ /
                                 kHighPercentileUpdateFrequency);
    approximate_high_percentile = worst_ten_latencies_[index];
  }
  return approximate_high_percentile;
}

absl::optional<base::TimeDelta>
ResponsivenessMetricsNormalization::worst_latency() const {
  absl::optional<base::TimeDelta> worst_latency;
  if (worst_ten_latencies_.size()) {
    worst_latency = worst_ten_latencies_[0];
  }
  return worst_latency;
}

void ResponsivenessMetricsNormalization::AddNewUserInteractionLatencies(
    uint64_t num_new_interactions,
    const mojom::UserInteractionLatencies& max_event_durations) {
  num_user_interactions_ += num_new_interactions;
  // Normalize max event durations.
  NormalizeUserInteractionLatencies(max_event_durations);
}

void ResponsivenessMetricsNormalization::ClearAllUserInteractionLatencies() {
  num_user_interactions_ = 0;
  worst_ten_latencies_ = std::vector<base::TimeDelta>();
}

void ResponsivenessMetricsNormalization::NormalizeUserInteractionLatencies(
    const mojom::UserInteractionLatencies& user_interaction_latencies) {
  DCHECK(user_interaction_latencies.is_user_interaction_latencies());

  // Insert each latency into the list if it is one of the worst ten seen so
  // far. Use inplace_merge to keep the list sorted after appending an element.
  for (const mojom::UserInteractionLatencyPtr& user_interaction :
       user_interaction_latencies.get_user_interaction_latencies()) {
    if (worst_ten_latencies_.size() < 10) {
      worst_ten_latencies_.push_back(user_interaction->interaction_latency);
    } else if (user_interaction->interaction_latency >
               worst_ten_latencies_.back()) {
      worst_ten_latencies_.back() = user_interaction->interaction_latency;
    } else {
      continue;
    }
    if (worst_ten_latencies_.size() > 1) {
      std::inplace_merge(worst_ten_latencies_.begin(),
                         std::prev(worst_ten_latencies_.end()),
                         worst_ten_latencies_.end(), std::greater<>());
    }
  }
}

}  // namespace page_load_metrics
