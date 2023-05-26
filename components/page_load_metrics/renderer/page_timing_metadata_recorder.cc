// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"

namespace page_load_metrics {
namespace {
bool IsTimeTicksRangeSensible(base::TimeTicks start, base::TimeTicks end) {
  return start <= end && end <= base::TimeTicks::Now();
}
}  // namespace

// The next instance id to use for a PageTimingMetadataRecorder.
int g_next_instance_id = 1;

PageTimingMetadataRecorder::MonotonicTiming::MonotonicTiming() = default;
PageTimingMetadataRecorder::MonotonicTiming::MonotonicTiming(
    const MonotonicTiming&) = default;
PageTimingMetadataRecorder::MonotonicTiming&
PageTimingMetadataRecorder::MonotonicTiming::operator=(const MonotonicTiming&) =
    default;
PageTimingMetadataRecorder::MonotonicTiming::MonotonicTiming(
    MonotonicTiming&&) = default;
PageTimingMetadataRecorder::MonotonicTiming&
PageTimingMetadataRecorder::MonotonicTiming::operator=(MonotonicTiming&&) =
    default;

PageTimingMetadataRecorder::PageTimingMetadataRecorder(
    const MonotonicTiming& initial_timing)
    : instance_id_(g_next_instance_id++) {
  UpdateMetadata(initial_timing);
}

PageTimingMetadataRecorder::~PageTimingMetadataRecorder() = default;

void PageTimingMetadataRecorder::UpdateMetadata(const MonotonicTiming& timing) {
  UpdateFirstContentfulPaintMetadata(timing.navigation_start,
                                     timing.first_contentful_paint);
  UpdateFirstInputDelayMetadata(timing.first_input_timestamp,
                                timing.first_input_delay);
  timing_ = timing;
}

void PageTimingMetadataRecorder::ApplyMetadataToPastSamples(
    base::TimeTicks period_start,
    base::TimeTicks period_end,
    base::StringPiece name,
    int64_t key,
    int64_t value,
    base::SampleMetadataScope scope) {
  base::ApplyMetadataToPastSamples(period_start, period_end, name, key, value,
                                   scope);
}

void PageTimingMetadataRecorder::UpdateFirstInputDelayMetadata(
    const absl::optional<base::TimeTicks>& first_input_timestamp,
    const absl::optional<base::TimeDelta>& first_input_delay) {
  // Applying metadata to past samples has non-trivial cost so only do so if
  // the relevant values changed.
  const bool should_apply_metadata =
      first_input_timestamp.has_value() && first_input_delay.has_value() &&
      (timing_.first_input_timestamp != first_input_timestamp ||
       timing_.first_input_delay != first_input_delay);

  if (should_apply_metadata && !first_input_delay->is_negative()) {
    ApplyMetadataToPastSamples(
        *first_input_timestamp, *first_input_timestamp + *first_input_delay,
        "PageLoad.InteractiveTiming.FirstInputDelay4", /* key=*/instance_id_,
        /* value=*/1, base::SampleMetadataScope::kProcess);
  }
}

void PageTimingMetadataRecorder::UpdateFirstContentfulPaintMetadata(
    const absl::optional<base::TimeTicks>& navigation_start,
    const absl::optional<base::TimeTicks>& first_contentful_paint) {
  // Applying metadata to past samples has non-trivial cost so only do so if
  // the relevant values changed.
  const bool should_apply_metadata =
      navigation_start.has_value() && first_contentful_paint.has_value() &&
      (timing_.navigation_start != navigation_start ||
       timing_.first_contentful_paint != first_contentful_paint);
  if (should_apply_metadata &&
      IsTimeTicksRangeSensible(*navigation_start, *first_contentful_paint)) {
    ApplyMetadataToPastSamples(
        *navigation_start, *first_contentful_paint,
        "PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
        /* key=*/instance_id_,
        /* value=*/1, base::SampleMetadataScope::kProcess);
  }
}

}  // namespace page_load_metrics
