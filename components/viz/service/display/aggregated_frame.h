// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_AGGREGATED_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_AGGREGATED_FRAME_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "components/viz/common/delegated_ink_metadata.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/latency/latency_info.h"

namespace viz {

class VIZ_SERVICE_EXPORT AggregatedFrame {
 public:
  AggregatedFrame();
  AggregatedFrame(AggregatedFrame&& other);
  ~AggregatedFrame();

  AggregatedFrame& operator=(AggregatedFrame&& other);

  // The visible height of the top-controls. If the value is not set, then the
  // visible height should be the same as in the latest submitted frame with a
  // value set.
  base::Optional<float> top_controls_visible_height;

  // A list of latency info used for this frame.
  std::vector<ui::LatencyInfo> latency_info;

  // Indicates the content color usage for this frame.
  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kSRGB;

  // Indicates whether any render passes have a copy output request.
  bool has_copy_requests = false;

  // Indicates whether this frame may contain video.
  bool may_contain_video = false;

  // This is the final root damage rect produced in
  // ProcessSurfaceOccludingDamage().
  // TODO(magchen@): This will be replaced by a damage rect list in the follow
  // up CL.
  gfx::Rect occluding_damage_;

  // Contains the metadata required for drawing a delegated ink trail onto the
  // end of a rendered ink stroke. This should only be present when two
  // conditions are met:
  //   1. The JS API |updateInkTrailStartPoint| is used - This gathers the
  //     metadata and puts it onto a compositor frame to be sent to viz.
  //   2. This frame will not be submitted to the root surface - The browser UI
  //     does not use this, and the frame must be contained within a
  //     SurfaceDrawQuad.
  // The ink trail created with this metadata will only last for a single frame
  // before it disappears, regardless of whether or not the next frame contains
  // delegated ink metadata.
  std::unique_ptr<DelegatedInkMetadata> delegated_ink_metadata;

  AggregatedRenderPassList render_pass_list;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_AGGREGATED_FRAME_H_
