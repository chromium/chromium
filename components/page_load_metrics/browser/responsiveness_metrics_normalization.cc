// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/responsiveness_metrics_normalization.h"

namespace page_load_metrics {

ResponsivenessMetricsNormalization::ResponsivenessMetricsNormalization() =
    default;
ResponsivenessMetricsNormalization::~ResponsivenessMetricsNormalization() =
    default;

std::optional<mojom::UserInteractionLatency>
ResponsivenessMetricsNormalization::ApproximateHighPercentile() const {
  std::optional<mojom::UserInteractionLatency> approximate_high_percentile;
  if (worst_ten_latencies_.size()) {
    uint64_t index =
        std::min(static_cast<uint64_t>(worst_ten_latencies_.size() - 1),
                 static_cast<uint64_t>(num_user_interactions_ /
                                       kHighPercentileUpdateFrequency));
    approximate_high_percentile = worst_ten_latencies_[index];
  }
  return approximate_high_percentile;
}

std::optional<mojom::UserInteractionLatency>
ResponsivenessMetricsNormalization::worst_latency() const {
  std::optional<mojom::UserInteractionLatency> worst_latency;
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
  worst_ten_latencies_ = std::vector<mojom::UserInteractionLatency>();
}

void ResponsivenessMetricsNormalization::NormalizeUserInteractionLatencies(
    const mojom::UserInteractionLatencies& user_interaction_latencies) {
  DCHECK(user_interaction_latencies.is_user_interaction_latencies());

  // Insert each latency into the list if it is one of the worst ten seen so
  // far. Use inplace_merge to keep the list sorted after appending an element.
  for (const mojom::UserInteractionLatencyPtr& user_interaction :
       user_interaction_latencies.get_user_interaction_latencies()) {
    if (worst_ten_latencies_.size() < 10) {
      worst_ten_latencies_.push_back(*user_interaction);
    } else if (user_interaction->interaction_latency >
               worst_ten_latencies_.back().interaction_latency) {
      worst_ten_latencies_.back() = *user_interaction;
    } else {
      continue;
    }
    if (worst_ten_latencies_.size() > 1) {
      std::inplace_merge(
          worst_ten_latencies_.begin(), std::prev(worst_ten_latencies_.end()),
          worst_ten_latencies_.end(),
          [](const auto& latency1, const auto& latency2) {
            return latency1.interaction_latency > latency2.interaction_latency;
          });
    }
  }
}

}  // namespace page_load_metrics
