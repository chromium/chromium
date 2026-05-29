// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"

namespace viz {

FrameDeadlineDecider::FrameDeadlineDecider() = default;

FrameDeadlineDecider::~FrameDeadlineDecider() = default;

size_t FrameDeadlineDecider::SelectDeadline(
    const PossibleDeadlines& possible_deadlines) {
  bool use_platform_preferred_deadlines = true;
#if BUILDFLAG(IS_ANDROID)
  use_platform_preferred_deadlines =
      !base::FeatureList::IsEnabled(features::kUseAndroidCustomFrameDeadlines);
#endif  // BUILDFLAG(IS_ANDROID)
  if (use_platform_preferred_deadlines ||
      possible_deadlines.deadlines.empty()) {
    return possible_deadlines.preferred_index;
  }

  if (in_frame_sequence_) {
    curr_sequence_deadline_index_ =
        FindClosestDeadlineByPresentation(possible_deadlines);
  } else {
    in_frame_sequence_ = true;
    curr_sequence_deadline_index_ = possible_deadlines.preferred_index;
  }
  curr_sequence_present_delta_ =
      possible_deadlines.deadlines[curr_sequence_deadline_index_].present_delta;
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
