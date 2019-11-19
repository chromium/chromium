// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_FRAME_DEADLINE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_FRAME_DEADLINE_H_

#include "components/viz/common/viz_common_export.h"

#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace viz {

// FrameDeadline is a class that represents a CompositorFrame's deadline for
// activation. The deadline consists of three components: start time, deadline
// in frames, and frame interval. All three components are stored individually
// in this class in order to allow resolution of this deadline that incorporates
// a system default deadline in the Viz service. In particular, the computation
// to translate FrameDeadline into wall time is:
// if use system default lower bound deadline:
//    start time + max(deadline in frames, default deadline) * frame interval
// else:
//    start time + deadline in frames * frame interval
class VIZ_COMMON_EXPORT FrameDeadline {
 public:
  static FrameDeadline MakeZero();

  FrameDeadline() = default;
  FrameDeadline(base::TimeTicks frame_start_time,
                uint32_t deadline_in_frames,
                base::TimeDelta frame_interval,
                bool use_default_lower_bound_deadline)
      : frame_start_time_(frame_start_time),
        deadline_in_frames_(deadline_in_frames),
        frame_interval_(frame_interval),
        use_default_lower_bound_deadline_(use_default_lower_bound_deadline) {}

  FrameDeadline(const FrameDeadline& other) = default;

  FrameDeadline& operator=(const FrameDeadline& other) = default;

  // Converts this FrameDeadline object into a wall time given a system default
  // deadline in frames.
  base::TimeTicks ToWallTime(
      base::Optional<uint32_t> default_deadline_in_frames =
          base::nullopt) const;

  bool operator==(const FrameDeadline& other) const {
    return other.frame_start_time_ == frame_start_time_ &&
           other.deadline_in_frames_ == deadline_in_frames_ &&
           other.frame_interval_ == frame_interval_ &&
           other.use_default_lower_bound_deadline_ ==
               use_default_lower_bound_deadline_;
  }

  bool operator!=(const FrameDeadline& other) const {
    return !(*this == other);
  }

  base::TimeTicks frame_start_time() const { return frame_start_time_; }

  uint32_t deadline_in_frames() const { return deadline_in_frames_; }

  base::TimeDelta frame_interval() const { return frame_interval_; }

  bool use_default_lower_bound_deadline() const {
    return use_default_lower_bound_deadline_;
  }

  bool IsZero() const;

  std::string ToString() const;

 private:
  base::TimeTicks frame_start_time_;
  uint32_t deadline_in_frames_ = 0u;
  base::TimeDelta frame_interval_ = BeginFrameArgs::DefaultInterval();
  bool use_default_lower_bound_deadline_ = true;
};

VIZ_COMMON_EXPORT std::ostream& operator<<(std::ostream& out,
                                           const FrameDeadline& frame_deadline);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_FRAME_DEADLINE_H_
