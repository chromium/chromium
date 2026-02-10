// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_INTERACTION_TO_NEXT_PAINT_CALCULATOR_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_INTERACTION_TO_NEXT_PAINT_CALCULATOR_H_

#include <limits>
#include <map>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

constexpr inline uint64_t kHighPercentileUpdateFrequency = 50;
constexpr inline uint64_t kMaxInteractions = 10;

// InteractionToNextPaintCalculator aggregates user interaction latency data
// from all renderer frames and picks a single representative interaction which
// is used to score the INP metrics per page load.
class InteractionToNextPaintCalculator {
 public:
  // Groups EventTimings for one common Interaction.
  // Each RenderFrameHost has its own independent list of interaction IDs, so
  // the [source_token, interaction_id] pair is needed to be used as a unique
  // key here.
  // Note: Interaction id is stored inside the EventTiming struct.
  struct InteractionData {
    // Store this token instead of using `RenderFrameHost*` directly.
    content::GlobalRenderFrameHostToken source_token;
    // We could store ALL EventTiming for this interaction, but we only use the
    // single longest duration event for INP.
    mojom::EventTiming max_event;
    // The one-based offset of this interaction in the order it was discovered
    // by this calculator. This is consistent with num_user_interactions().
    // Note: while usually unique, this is not guaranteed to be strictly
    // sequential if interactions are reported out of order.
    uint64_t interaction_offset;
  };

  InteractionToNextPaintCalculator();
  InteractionToNextPaintCalculator(const InteractionToNextPaintCalculator&) =
      delete;
  InteractionToNextPaintCalculator& operator=(
      const InteractionToNextPaintCalculator&) = delete;

  ~InteractionToNextPaintCalculator();

  void AddNewEventTimings(
      const content::RenderFrameHost& source,
      base::span<const mojom::EventTimingPtr> event_timings);

  void ClearEventTimings();

  // Approximate a high percentile of user interaction latency.
  std::optional<InteractionData> ApproximateHighPercentile() const;
  std::optional<InteractionData> worst_latency() const;

  // The number of user interactions is computed as the sum of the ranges of
  // interaction IDs seen from each renderer frame. This assumes interaction
  // IDs are consecutive and 1-based, though we handle arbitrary starting IDs
  // for soft navigations by tracking the min and max ID per source.
  uint64_t num_user_interactions() const { return num_user_interactions_; }

 private:
  struct InteractionIdRange {
    uint64_t min = 0;
    uint64_t max = 0;
  };

  void AddNewEventTiming(
      const content::GlobalRenderFrameHostToken& source_token,
      const mojom::EventTiming& event);

  void TrimEventTimings();

  // A sorted list of the worst ten interactions, used to approximate a high
  // percentile.
  std::vector<InteractionData> event_timings_;
  std::map<content::GlobalRenderFrameHostToken, InteractionIdRange>
      interaction_id_ranges_per_source_;
  uint64_t num_user_interactions_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_INTERACTION_TO_NEXT_PAINT_CALCULATOR_H_
