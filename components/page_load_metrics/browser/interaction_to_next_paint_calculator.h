// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_INTERACTION_TO_NEXT_PAINT_CALCULATOR_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_INTERACTION_TO_NEXT_PAINT_CALCULATOR_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace page_load_metrics {

constexpr inline uint64_t kHighPercentileUpdateFrequency = 50;
// InteractionToNextPaintCalculator implements some experimental
// normalization strategies for responsiveness metrics. We aggregate user
// interaction latency data from all renderer frames and calculate a score per
// page load.
class InteractionToNextPaintCalculator {
 public:
  InteractionToNextPaintCalculator();
  InteractionToNextPaintCalculator(const InteractionToNextPaintCalculator&) =
      delete;
  InteractionToNextPaintCalculator& operator=(
      const InteractionToNextPaintCalculator&) = delete;

  ~InteractionToNextPaintCalculator();

  void AddNewEventTimings(
      base::span<const mojom::EventTimingPtr> event_timings);

  void ClearEventTimings();

  // Merges the other_interactions InteractionToNextPaintCalculator object
  // into this one while clearing the other object.
  void MergeAndClear(InteractionToNextPaintCalculator* other_interactions);

  // Approximate a high percentile of user interaction latency.
  std::optional<mojom::EventTiming> ApproximateHighPercentile() const;

  uint64_t num_user_interactions() const { return num_user_interactions_; }

  std::optional<mojom::EventTiming> worst_latency() const;

 private:
  void NormalizeUserInteractionLatencies(
      base::span<const mojom::EventTimingPtr> event_timings);

  // A sorted list of the worst ten latencies, used to approximate a high
  // percentile.
  std::vector<mojom::EventTimingPtr> event_timings_;
  uint64_t num_user_interactions_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_INTERACTION_TO_NEXT_PAINT_CALCULATOR_H_
