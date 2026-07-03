// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_DEADLINE_DECIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_DEADLINE_DECIDER_H_

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT FrameDeadlineDecider {
 public:
  // Input latency beyond this threshold is perceptible to the user.
  static constexpr base::TimeDelta kPerceptibleLatencyThreshold =
      base::Milliseconds(100);

  explicit FrameDeadlineDecider(bool use_platform_preferred_deadlines);
  ~FrameDeadlineDecider();

  FrameDeadlineDecider(const FrameDeadlineDecider&) = delete;
  FrameDeadlineDecider& operator=(const FrameDeadlineDecider&) = delete;

  // Queries the best deadline index for the given parameters without modifying
  // any internal state of the decider. This is safe to call multiple times or
  // from const methods.
  size_t QueryDeadline(
      const PossibleDeadlines& possible_deadlines,
      base::TimeDelta vsync_interval,
      int max_allowed_buffers,
      base::TimeTicks frame_time,
      std::optional<base::TimeTicks> earliest_input_time) const;

  // Selects the best deadline index and updates the internal state of the
  // decider to lock to the selected deadline for the current sequence.
  // This should only be called once per frame when we are actually going to
  // draw. It differs from QueryDeadline in that it has side effects on the
  // internal sequence tracking state.
  size_t SelectDeadline(const PossibleDeadlines& possible_deadlines,
                        base::TimeDelta vsync_interval,
                        int max_allowed_buffers,
                        base::TimeTicks frame_time,
                        std::optional<base::TimeTicks> earliest_input_time);

  // Called when the display scheduler goes idle or invisible, to reset sequence
  // state.
  void OnGoIdle();

 private:
  size_t FindClosestDeadlineByPresentation(
      const PossibleDeadlines& possible_deadlines) const;

  bool in_frame_sequence_ = false;
  base::TimeDelta curr_sequence_present_delta_;
  size_t curr_sequence_deadline_index_ = 0;
  const bool use_platform_preferred_deadlines_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_DEADLINE_DECIDER_H_
