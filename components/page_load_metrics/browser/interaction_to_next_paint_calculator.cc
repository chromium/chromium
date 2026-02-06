// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/interaction_to_next_paint_calculator.h"

namespace page_load_metrics {

InteractionToNextPaintCalculator::InteractionToNextPaintCalculator() = default;
InteractionToNextPaintCalculator::~InteractionToNextPaintCalculator() = default;

std::optional<mojom::EventTiming>
InteractionToNextPaintCalculator::ApproximateHighPercentile() const {
  std::optional<mojom::EventTiming> approximate_high_percentile;
  if (event_timings_.size()) {
    uint64_t index =
        std::min(static_cast<uint64_t>(event_timings_.size() - 1),
                 static_cast<uint64_t>(num_user_interactions_ /
                                       kHighPercentileUpdateFrequency));
    approximate_high_percentile = *event_timings_[index];
  }
  return approximate_high_percentile;
}

std::optional<mojom::EventTiming>
InteractionToNextPaintCalculator::worst_latency() const {
  std::optional<mojom::EventTiming> worst_latency;
  if (event_timings_.size()) {
    worst_latency = *event_timings_[0];
  }
  return worst_latency;
}

void InteractionToNextPaintCalculator::AddNewEventTimings(
    base::span<const mojom::EventTimingPtr> event_timings) {
  num_user_interactions_ += event_timings.size();
  // Normalize max event durations.
  NormalizeUserInteractionLatencies(event_timings);
}

void InteractionToNextPaintCalculator::ClearEventTimings() {
  num_user_interactions_ = 0;
  event_timings_.clear();
}

void InteractionToNextPaintCalculator::MergeAndClear(

    InteractionToNextPaintCalculator* other_interactions) {
  num_user_interactions_ += other_interactions->num_user_interactions_;
  NormalizeUserInteractionLatencies(other_interactions->event_timings_);
  other_interactions->ClearEventTimings();
}

void InteractionToNextPaintCalculator::NormalizeUserInteractionLatencies(
    base::span<const mojom::EventTimingPtr> event_timings) {
  // Insert each latency into the list if it is one of the worst ten seen so
  // far. Use inplace_merge to keep the list sorted after appending an element.
  for (const mojom::EventTimingPtr& user_interaction : event_timings) {
    if (event_timings_.size() < 10) {
      event_timings_.push_back(user_interaction->Clone());
    } else if (user_interaction->duration > event_timings_.back()->duration) {
      event_timings_.back() = user_interaction->Clone();
    } else {
      continue;
    }
    if (event_timings_.size() > 1) {
      std::inplace_merge(event_timings_.begin(),
                         std::prev(event_timings_.end()), event_timings_.end(),
                         [](const auto& latency1, const auto& latency2) {
                           return latency1->duration > latency2->duration;
                         });
    }
  }
}

}  // namespace page_load_metrics
