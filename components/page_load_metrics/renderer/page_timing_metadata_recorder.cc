// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"

#include "base/profiler/sample_metadata.h"

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
  // Applying metadata to past samples has non-trivial cost so only do so if
  // the relevant values changed.
  const bool should_apply_metadata =
      timing.navigation_start.has_value() &&
      timing.first_contentful_paint.has_value() &&
      (timing_.navigation_start != timing.navigation_start ||
       timing_.first_contentful_paint != timing.first_contentful_paint);
  if (should_apply_metadata &&
      IsTimeTicksRangeSensible(*timing.navigation_start,
                               *timing.first_contentful_paint)) {
    base::ApplyMetadataToPastSamples(
        *timing.navigation_start, *timing.first_contentful_paint,
        "PageLoad.PaintTiming.NavigationToFirstContentfulPaint", instance_id_,
        1);
  }

  timing_ = timing;
}

}  // namespace page_load_metrics
