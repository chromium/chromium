// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_AGGREGATED_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_AGGREGATED_FRAME_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/latency/latency_info.h"

namespace viz {

typedef std::vector<gfx::Rect> SurfaceDamageRectList;

class VIZ_SERVICE_EXPORT AggregatedFrame {
 public:
  AggregatedFrame();
  AggregatedFrame(AggregatedFrame&& other);
  ~AggregatedFrame();

  AggregatedFrame& operator=(AggregatedFrame&& other);

  void AsValueInto(base::trace_event::TracedValue* value) const;
  std::string ToString() const;

  // A list of latency info used for this frame.
  std::vector<ui::LatencyInfo> latency_info;

  // Indicates the content color usage for this frame.
  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kSRGB;

  // Indicates whether any render passes have a copy output request.
  bool has_copy_requests = false;

  // Indicates whether this is a page fullscreen mode without Chrome tabs. When
  // in the page fullscreen mode, the content surface has the same size as the
  // root render pass |output_rect| (display size) on the root surface.
  bool page_fullscreen_mode = false;

  // A list of surface damage rects in the current frame, used for overlays.
  SurfaceDamageRectList surface_damage_rect_list_;

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
  std::unique_ptr<gfx::DelegatedInkMetadata> delegated_ink_metadata;

  AggregatedRenderPassList render_pass_list;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_AGGREGATED_FRAME_H_
