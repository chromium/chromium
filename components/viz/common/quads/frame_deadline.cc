// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/frame_deadline.h"

#include <cinttypes>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace viz {

// static
FrameDeadline FrameDeadline::MakeZero() {
  return FrameDeadline(base::TimeTicks(), 0, base::TimeDelta(), false);
}

base::TimeTicks FrameDeadline::ToWallTime(
    base::Optional<uint32_t> default_deadline_in_frames) const {
  uint32_t deadline_in_frames = deadline_in_frames_;
  if (use_default_lower_bound_deadline_) {
    deadline_in_frames =
        std::max(deadline_in_frames, default_deadline_in_frames.value_or(
                                         std::numeric_limits<uint32_t>::max()));
  }
  return frame_start_time_ + deadline_in_frames * frame_interval_;
}

bool FrameDeadline::IsZero() const {
  return deadline_in_frames_ == 0 && !use_default_lower_bound_deadline_;
}

std::string FrameDeadline::ToString() const {
  const base::TimeDelta start_time_delta =
      frame_start_time_ - base::TimeTicks();
  return base::StringPrintf(
      "FrameDeadline(start time: %" PRId64
      " ms, deadline in frames: %s, frame interval: %" PRId64 " ms)",
      start_time_delta.InMilliseconds(),
      use_default_lower_bound_deadline_
          ? "unresolved"
          : base::NumberToString(deadline_in_frames_).c_str(),
      frame_interval_.InMilliseconds());
}

std::ostream& operator<<(std::ostream& out,
                         const FrameDeadline& frame_deadline) {
  return out << frame_deadline.ToString();
}

}  // namespace viz
