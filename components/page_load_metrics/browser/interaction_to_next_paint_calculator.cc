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

std::optional<mojom::EventTiming>
InteractionToNextPaintCalculator::ApproximateHighPercentile() const {
  if (event_timings_.empty()) {
    return std::nullopt;
  }
  uint64_t index =
      std::min(static_cast<uint64_t>(event_timings_.size() - 1),
               static_cast<uint64_t>(num_user_interactions_ /
                                     kHighPercentileUpdateFrequency));
  return event_timings_[index].max_event;
}

std::optional<mojom::EventTiming>
InteractionToNextPaintCalculator::worst_latency() const {
  if (event_timings_.empty()) {
    return std::nullopt;
  }
  return event_timings_[0].max_event;
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
    if (event_timing->interaction_id == 0) {
      continue;
    }
    range.min = std::min(range.min, event_timing->interaction_id);
    range.max = std::max(range.max, event_timing->interaction_id);
    AddNewEventTiming(source_token, *event_timing);
  }

  UpdateNumUserInteractions();
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
  event_timings_.push_back({source_token, event});
}

void InteractionToNextPaintCalculator::ClearEventTimings() {
  num_user_interactions_ = 0;
  event_timings_.clear();
  interaction_id_ranges_per_source_.clear();
}

void InteractionToNextPaintCalculator::MergeAndClear(
    InteractionToNextPaintCalculator* other_interactions) {
  for (const auto& [source_token, other_range] :
       other_interactions->interaction_id_ranges_per_source_) {
    InteractionIdRange& range = interaction_id_ranges_per_source_[source_token];
    range.min = std::min(range.min, other_range.min);
    range.max = std::max(range.max, other_range.max);
  }

  for (auto& other_interaction : other_interactions->event_timings_) {
    AddNewEventTiming(other_interaction.source_token,
                      other_interaction.max_event);
  }
  UpdateNumUserInteractions();
  TrimEventTimings();
  other_interactions->ClearEventTimings();
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

void InteractionToNextPaintCalculator::UpdateNumUserInteractions() {
  num_user_interactions_ = 0;
  for (const auto& [source_token, range] : interaction_id_ranges_per_source_) {
    if (range.min == std::numeric_limits<uint64_t>::max()) {
      continue;
    }
    CHECK(range.min != 0);
    CHECK(range.max != 0);
    CHECK(range.max >= range.min);
    num_user_interactions_ += (range.max - range.min + 1);
  }
}

}  // namespace page_load_metrics
