// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/interaction_to_next_paint_calculator.h"

#include <algorithm>
#include <limits>

#include "content/public/browser/render_frame_host.h"

namespace page_load_metrics {

InteractionToNextPaintCalculator::InteractionToNextPaintCalculator() = default;
InteractionToNextPaintCalculator::~InteractionToNextPaintCalculator() = default;

std::optional<InteractionToNextPaintCalculator::InteractionData>
InteractionToNextPaintCalculator::ApproximateHighPercentile() const {
  if (event_timings_.empty()) {
    return std::nullopt;
  }
  uint64_t index =
      std::min(static_cast<uint64_t>(event_timings_.size() - 1),
               static_cast<uint64_t>(num_user_interactions_ /
                                     kHighPercentileUpdateFrequency));
  return event_timings_[index];
}

std::optional<InteractionToNextPaintCalculator::InteractionData>
InteractionToNextPaintCalculator::worst_latency() const {
  if (event_timings_.empty()) {
    return std::nullopt;
  }
  return event_timings_[0];
}

void InteractionToNextPaintCalculator::AddNewEventTimings(
    const content::RenderFrameHost& source,
    base::span<const mojom::EventTimingPtr> event_timings) {
  if (event_timings.empty()) {
    return;
  }

  content::GlobalRenderFrameHostToken source_token =
      source.GetGlobalFrameToken();

  InteractionIdRange& range = interaction_id_ranges_per_source_[source_token];
  for (const auto& event_timing : event_timings) {
    uint64_t id = event_timing->interaction_id;
    if (id == 0) {
      // Interactions should never have id == 0, but we should still not depend
      // that Renderers will never send such values.
      continue;
    }

    // Usually ids are reported in-order and increment by 1.
    // But there are cases where ids could be skipped or be reported out of
    // order, so try to be resilient to this.
    if (range.max == 0) {  // First interaction for this renderer
      num_user_interactions_++;
      range.min = id;
      range.max = id;
    } else if (id > range.max) {  // Next interaction
      num_user_interactions_ += (id - range.max);
      range.max = id;
    } else if (id < range.min) {  // Rare: older interaction reported late
      num_user_interactions_ += (range.min - id);
      range.min = id;
    }

    AddNewEventTiming(source_token, *event_timing);
  }

  TrimEventTimings();
}

void InteractionToNextPaintCalculator::AddNewEventTiming(
    const content::GlobalRenderFrameHostToken& source_token,
    const mojom::EventTiming& event) {
  for (auto& interaction : event_timings_) {
    if (interaction.source_token == source_token &&
        interaction.max_event.interaction_id == event.interaction_id) {
      // It's possible to receive multiple events for the same interaction ID
      // (e.g., pointerdown followed by pointerup). We only care about the
      // longest duration event for that interaction.
      if (event.duration > interaction.max_event.duration) {
        interaction.max_event = event;
      }
      return;
    }
  }
  event_timings_.push_back({source_token, event, num_user_interactions_});
}

void InteractionToNextPaintCalculator::ClearEventTimings() {
  num_user_interactions_ = 0;
  event_timings_.clear();
  interaction_id_ranges_per_source_.clear();
}

void InteractionToNextPaintCalculator::TrimEventTimings() {
  std::sort(event_timings_.begin(), event_timings_.end(),
            [](const auto& a, const auto& b) {
              return a.max_event.duration > b.max_event.duration;
            });
  if (event_timings_.size() > kMaxInteractions) {
    event_timings_.erase(std::next(event_timings_.begin(), kMaxInteractions),
                         event_timings_.end());
  }
}

}  // namespace page_load_metrics
