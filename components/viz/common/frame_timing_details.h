// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_
#define COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_

#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace viz {

class VIZ_COMMON_EXPORT FrameTimingDetails {
 public:
  FrameTimingDetails();
  FrameTimingDetails(const FrameTimingDetails& other);
  ~FrameTimingDetails();
  // The time when the frame submitted by the client is received by the Viz
  // service. If this frame corresponds to a non-root Surface, it will not be
  // drawn until it is referenced by a parent Surface.
  base::TimeTicks received_compositor_frame_timestamp;
  // The time when the frame submitted by the client is embedded by a parent
  // Surface. Maybe the same as `received_compositor_frame_timestamp` for the
  // root surface.
  base::TimeTicks embedded_frame_timestamp;
  base::TimeTicks draw_start_timestamp;
  gfx::SwapTimings swap_timings;
  gfx::PresentationFeedback presentation_feedback;
  BeginFrameId frame_id;

  // Optional TreesInViz timing details.
  // TODO(crbug.com/430288888): Make use of these timing breakdowns in
  // CompositorFrameReporter Viz Breakdowns.
  base::TimeTicks start_update_display_tree;
  base::TimeTicks start_prepare_to_draw;
  base::TimeTicks start_draw_layers;
  base::TimeTicks submit_compositor_frame;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_
