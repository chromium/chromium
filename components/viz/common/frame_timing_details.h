// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_
#define COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_

#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace viz {

struct FrameTimingDetails {
  base::TimeTicks received_compositor_frame_timestamp;
  base::TimeTicks draw_start_timestamp;
  gfx::SwapTimings swap_timings;
  gfx::PresentationFeedback presentation_feedback;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_
