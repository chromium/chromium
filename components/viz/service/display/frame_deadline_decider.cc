// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include <algorithm>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace viz {

FrameDeadlineDecider::FrameDeadlineDecider() = default;

FrameDeadlineDecider::~FrameDeadlineDecider() = default;

size_t FrameDeadlineDecider::SelectDeadline(
    const PossibleDeadlines& possible_deadlines,
    base::TimeDelta vsync_interval,
    int max_pending_swaps,
    base::TimeTicks frame_time,
    std::optional<base::TimeTicks> earliest_input_time) {
  bool use_platform_preferred_deadlines = true;
#if BUILDFLAG(IS_ANDROID)
  use_platform_preferred_deadlines =
      !base::FeatureList::IsEnabled(features::kUseAndroidCustomFrameDeadlines);
#endif  // BUILDFLAG(IS_ANDROID)
  if (use_platform_preferred_deadlines ||
      possible_deadlines.deadlines.empty()) {
    return possible_deadlines.os_preferred_index;
  }

  absl::Cleanup update_present_delta = [this, &possible_deadlines] {
    curr_sequence_present_delta_ =
        possible_deadlines.deadlines[curr_sequence_deadline_index_]
            .present_delta;
  };

  if (in_frame_sequence_) {
    curr_sequence_deadline_index_ =
        FindClosestDeadlineByPresentation(possible_deadlines);
    return curr_sequence_deadline_index_;
  }

  in_frame_sequence_ = true;
  int presentation_offset = 0;
#if BUILDFLAG(IS_ANDROID)
  presentation_offset =
      features::kAndroidCustomFrameDeadlinePresentationOffset.Get();
#endif  // BUILDFLAG(IS_ANDROID)

  int num_buffers = max_pending_swaps + 1;
  // num_buffers * vsync_interval is the maximum presentation interval we would
  // want to target. Since going beyond this threshold means frames would now
  // start stalling for long time, waiting for buffers to be freed. Thus,
  // `presentation_offset` is expected to be non-positive (<= 0).
  int target_present_multiplier = num_buffers + presentation_offset;
  CHECK_GT(target_present_multiplier, 0);
  base::TimeDelta target_present_delta =
      target_present_multiplier * vsync_interval;

  if (earliest_input_time.has_value()) {
    // The earliest input time can be in the future relative to frame_time
    // in cases like WaitForLateScroll where we wait for input events to
    // arrive after the begin frame is sent. Clamp to 0 in such cases.
    const base::TimeDelta input_delta =
        std::max(base::TimeDelta(), frame_time - *earliest_input_time);
    // We subtract 1.25 * vsync_interval from the perceptible latency threshold
    // to allow a safety buffer for potential OS side frame jank.
    const base::TimeDelta latency_cap =
        kPerceptibleLatencyThreshold - vsync_interval - (vsync_interval / 4);
    const base::TimeDelta max_present_delta = latency_cap - input_delta;
    if (max_present_delta < target_present_delta) {
      // Reduce target presentation delta to pull the deadline earlier and
      // satisfy the perceptible input-latency threshold constraints.
      target_present_delta = max_present_delta;
    }
  }

  auto it = std::upper_bound(
      possible_deadlines.deadlines.begin(), possible_deadlines.deadlines.end(),
      target_present_delta,
      [](base::TimeDelta target, const PossibleDeadline& deadline) {
        return target < deadline.present_delta;
      });

  if (it != possible_deadlines.deadlines.begin()) {
    --it;
  }

  const size_t chrome_preferred_index =
      std::distance(possible_deadlines.deadlines.begin(), it);
  const PossibleDeadline& chrome_preferred_deadline = *it;

  if (chrome_preferred_deadline.present_delta > target_present_delta) {
    curr_sequence_deadline_index_ = possible_deadlines.os_preferred_index;
    return curr_sequence_deadline_index_;
  }

  if (chrome_preferred_deadline.present_delta <
      possible_deadlines.GetOSPreferredDeadline().present_delta) {
    // Fallback to os preferred deadline instead of reducing the preferred
    // deadline. We are not sure if this would actually happen in field.
    curr_sequence_deadline_index_ = possible_deadlines.os_preferred_index;
    return curr_sequence_deadline_index_;
  }

  curr_sequence_deadline_index_ = chrome_preferred_index;
  return curr_sequence_deadline_index_;
}

void FrameDeadlineDecider::OnGoIdle() {
  // TODO(crbug.com/500826814): Handle cases where scheduler goes to idle and
  // then immediately kicks off again, so we don't break the frame sequence.
  in_frame_sequence_ = false;
  curr_sequence_present_delta_ = base::TimeDelta();
  curr_sequence_deadline_index_ = 0;
}

size_t FrameDeadlineDecider::FindClosestDeadlineByPresentation(
    const PossibleDeadlines& possible_deadlines) const {
  // Check if the cached index is valid and within 1ms of target.
  if (curr_sequence_deadline_index_ < possible_deadlines.deadlines.size()) {
    const auto& cached_deadline =
        possible_deadlines.deadlines[curr_sequence_deadline_index_];
    if ((cached_deadline.present_delta - curr_sequence_present_delta_)
            .magnitude() <= base::Milliseconds(1)) {
      return curr_sequence_deadline_index_;
    }
  }

  // We are trying to uphold the presentation deadline being used by the
  // previous frame in the sequence. Initializing the search with the 0th index
  // is perfectly fine for the baseline comparison.
  size_t best_index = 0;
  base::TimeDelta min_diff = (possible_deadlines.deadlines[0].present_delta -
                              curr_sequence_present_delta_)
                                 .magnitude();

  // Possible deadlines are guaranteed to be in chronological order from
  // Android.
  for (size_t i = 1; i < possible_deadlines.deadlines.size(); ++i) {
    const auto& deadline = possible_deadlines.deadlines[i];
    base::TimeDelta diff =
        (deadline.present_delta - curr_sequence_present_delta_).magnitude();
    if (diff < min_diff) {
      min_diff = diff;
      best_index = i;
    }
  }
  return best_index;
}

}  // namespace viz
