// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESPONSIVENESS_METRICS_NORMALIZATION_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESPONSIVENESS_METRICS_NORMALIZATION_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace page_load_metrics {

constexpr uint64_t kHighPercentileUpdateFrequency = 50;
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
      base::span<const mojom::UserInteractionLatencyPtr> max_event_durations);

  void ClearAllUserInteractionLatencies();

  // Merges the other_interactions ResponsivenessMetricsNormalization object
  // into this one while clearing the other object.
  void MergeAndClear(ResponsivenessMetricsNormalization* other_interactions);

  // Approximate a high percentile of user interaction latency.
  std::optional<mojom::UserInteractionLatency> ApproximateHighPercentile()
      const;

  uint64_t num_user_interactions() const { return num_user_interactions_; }

  std::optional<mojom::UserInteractionLatency> worst_latency() const;

 private:
  void NormalizeUserInteractionLatencies(
      base::span<const mojom::UserInteractionLatencyPtr>
          user_interaction_latencies);

  // A sorted list of the worst ten latencies, used to approximate a high
  // percentile.
  std::vector<mojom::UserInteractionLatencyPtr> worst_ten_latencies_;
  uint64_t num_user_interactions_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_RESPONSIVENESS_METRICS_NORMALIZATION_H_
