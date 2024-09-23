// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"

#include <cstdint>
#include <string_view>

namespace page_load_metrics {
namespace {
bool IsTimeTicksRangeSensible(base::TimeTicks start, base::TimeTicks end) {
  return start <= end && end <= base::TimeTicks::Now();
}
}  // namespace

// The next instance id to use for a PageTimingMetadataRecorder.
uint32_t g_next_instance_id = 1;

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
    const MonotonicTiming& initial_timing,
    const bool is_main_frame)
    : instance_id_(g_next_instance_id++), is_main_frame_(is_main_frame) {
  UpdateMetadata(initial_timing);
}

PageTimingMetadataRecorder::~PageTimingMetadataRecorder() = default;

void PageTimingMetadataRecorder::UpdateMetadata(const MonotonicTiming& timing) {
  UpdateFirstContentfulPaintMetadata(timing.navigation_start,
                                     timing.first_contentful_paint);
  UpdateFirstInputDelayMetadata(timing.first_input_timestamp,
                                timing.first_input_delay);
  UpdateLargestContentfulPaintMetadata(timing.navigation_start,
                                       timing.frame_largest_contentful_paint);
  timing_ = timing;
}

void PageTimingMetadataRecorder::ApplyMetadataToPastSamples(
    base::TimeTicks period_start,
    base::TimeTicks period_end,
    std::string_view name,
    int64_t key,
    int64_t value,
    base::SampleMetadataScope scope) {
  base::ApplyMetadataToPastSamples(period_start, period_end, name, key, value,
                                   scope);
}

void PageTimingMetadataRecorder::UpdateFirstInputDelayMetadata(
    const std::optional<base::TimeTicks>& first_input_timestamp,
    const std::optional<base::TimeDelta>& first_input_delay) {
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
    const std::optional<base::TimeTicks>& navigation_start,
    const std::optional<base::TimeTicks>& first_contentful_paint) {
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

int64_t PageTimingMetadataRecorder::CreateInteractionDurationMetadataKey(
    const uint32_t instance_id,
    const uint32_t interaction_id) {
  // Constructing the key as unsigned int to avoid signed wraparound issues.
  const uint64_t composite_uint =
      (static_cast<uint64_t>(instance_id) << 32) | interaction_id;
  return static_cast<int64_t>(composite_uint);
}

void PageTimingMetadataRecorder::AddInteractionDurationMetadata(
    const base::TimeTicks interaction_start,
    const base::TimeTicks interaction_end) {
  if (!IsTimeTicksRangeSensible(interaction_start, interaction_end)) {
    return;
  }

  interaction_count_++;
  ApplyMetadataToPastSamples(
      interaction_start, interaction_end,
      "Blink.Responsiveness.UserInteraction.MaxEventDuration",
      /* key=*/
      CreateInteractionDurationMetadataKey(instance_id_, interaction_count_),
      /* value=*/(interaction_end - interaction_start).InMilliseconds(),
      base::SampleMetadataScope::kProcess);
}

void PageTimingMetadataRecorder::AddInteractionDurationAfterQueueingMetadata(
    const base::TimeTicks interaction_start,
    const base::TimeTicks interaction_queued_main_thread,
    const base::TimeTicks interaction_commit_finish,
    const base::TimeTicks interaction_end) {
  // Fallback to presentation time if commit finish timestamp is not available.
  // This could happen if features::kNonBlockingCommit is disabled or when an
  // interaction does not need a next paint.
  base::TimeTicks commit_finish_time_with_fallback;
  if (interaction_commit_finish == base::TimeTicks()) {
    commit_finish_time_with_fallback = interaction_end;
  } else {
    commit_finish_time_with_fallback = interaction_commit_finish;
  }

  // Safe check that start < queued < commit < end.
  if (!IsTimeTicksRangeSensible(interaction_start,
                                interaction_queued_main_thread) ||
      !IsTimeTicksRangeSensible(interaction_queued_main_thread,
                                commit_finish_time_with_fallback) ||
      !IsTimeTicksRangeSensible(commit_finish_time_with_fallback,
                                interaction_end)) {
    return;
  }

  ApplyMetadataToPastSamples(
      interaction_queued_main_thread, commit_finish_time_with_fallback,
      "Blink.Responsiveness.UserInteraction."
      "MaxEventDurationFromQueuedToCommitFinish",
      /* key=*/
      CreateInteractionDurationMetadataKey(instance_id_, interaction_count_),
      /* value=*/
      (commit_finish_time_with_fallback - interaction_queued_main_thread)
          .InMilliseconds(),
      base::SampleMetadataScope::kThread);
}

void PageTimingMetadataRecorder::UpdateLargestContentfulPaintMetadata(
    const std::optional<base::TimeTicks>& navigation_start,
    const std::optional<base::TimeTicks>& largest_contentful_paint) {
  // Local LCP can get updated multiple times (mostly < 10 times) during a page
  // load. For a given `name_hash` and `key`, when applying on new LCP range,
  // the metadata tag on old overlapping ranges will be removed.
  const bool should_apply_local_lcp_metadata =
      navigation_start.has_value() && largest_contentful_paint.has_value() &&
      (timing_.frame_largest_contentful_paint != largest_contentful_paint ||
       timing_.navigation_start != navigation_start);

  if (should_apply_local_lcp_metadata &&
      IsTimeTicksRangeSensible(*navigation_start, *largest_contentful_paint)) {
    ApplyMetadataToPastSamples(
        *navigation_start, *largest_contentful_paint,
        is_main_frame_ ? "PageLoad.PaintTiming."
                         "NavigationToLargestContentfulPaint2.MainFrame"
                       : "PageLoad.PaintTiming."
                         "NavigationToLargestContentfulPaint2.SubFrame",
        /* key=*/instance_id_,
        /* value=*/
        (*largest_contentful_paint - *navigation_start).InMilliseconds(),
        base::SampleMetadataScope::kProcess);
  }
}

}  // namespace page_load_metrics
