// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESPONSIVENESS_METRICS_NORMALIZATION_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESPONSIVENESS_METRICS_NORMALIZATION_H_

#include <queue>

#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace page_load_metrics {

constexpr uint64_t kHighPercentileUpdateFrequency = 50;
// The struct that stores normalized user interactions latencies.
struct NormalizedInteractionLatencies {
  NormalizedInteractionLatencies();
  ~NormalizedInteractionLatencies();

  // The maximum value of user interaction latencies.
  base::TimeDelta worst_latency;
  // For metrics below, we reduce a fixed budget from every user interaction
  // latency and we call it latency over budget. Then we calculaute the
  // worst(maximum), the sum, and an approximation of high quantile.
  base::TimeDelta worst_latency_over_budget;
  base::TimeDelta sum_of_latency_over_budget;
  base::TimeDelta high_percentile_latency_over_budget;

  // A min priority queue. The top is the smallest base::TimeDelta in the queue.
  // We use the worst 10 latencies to approximate a high percentile.
  std::priority_queue<base::TimeDelta,
                      std::vector<base::TimeDelta>,
                      std::greater<>>
      worst_ten_latencies;

  // A min priority queue. The top is the smallest base::TimeDelta in the queue.
  // We use the worst 10 latencies over budget to approximate a high percentile.
  std::priority_queue<base::TimeDelta,
                      std::vector<base::TimeDelta>,
                      std::greater<>>
      worst_ten_latencies_over_budget;
};

// The struct that stores all normalization results for a page load.
struct NormalizedResponsivenessMetrics {
  NormalizedResponsivenessMetrics();
  ~NormalizedResponsivenessMetrics();
  uint64_t num_user_interactions = 0;
  // Max event duration
  NormalizedInteractionLatencies normalized_max_event_durations;

  // Total event duration
  NormalizedInteractionLatencies normalized_total_event_durations;
};

// ResponsivenessMetricsNormalization implements some experimental normalization
// strategies for responsiveness metrics. We aggregate user interaction latency
// data from all renderer frames and calculate a score per page load.
class ResponsivenessMetricsNormalization {
 public:
  ResponsivenessMetricsNormalization();
  ResponsivenessMetricsNormalization(
      const ResponsivenessMetricsNormalization&) = delete;
  ResponsivenessMetricsNormalization& operator=(
      const ResponsivenessMetricsNormalization&) = delete;

  ~ResponsivenessMetricsNormalization();

  void AddNewUserInteractionLatencies(
      uint64_t num_new_interactions,
      const mojom::UserInteractionLatencies& max_event_durations);

  const NormalizedResponsivenessMetrics& GetNormalizedResponsivenessMetrics()
      const {
    return normalized_responsiveness_metrics_;
  }
  void ClearAllUserInteractionLatencies() {
    normalized_responsiveness_metrics_ = NormalizedResponsivenessMetrics();
  }

  // Approximate a high percentile of user interaction latency.
  static base::TimeDelta ApproximateHighPercentile(
      uint64_t num_interactions,
      std::priority_queue<base::TimeDelta,
                          std::vector<base::TimeDelta>,
                          std::greater<>> worst_ten_latencies);

 private:
  void NormalizeUserInteractionLatencies(
      const mojom::UserInteractionLatencies& user_interaction_latencies,
      NormalizedInteractionLatencies& normalized_event_durations,
      uint64_t last_num_user_interactions,
      uint64_t current_num_user_interactions);
  NormalizedResponsivenessMetrics normalized_responsiveness_metrics_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESPONSIVENESS_METRICS_NORMALIZATION_H_
