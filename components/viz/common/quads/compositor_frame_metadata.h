// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_METADATA_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_METADATA_H_

#include <stdint.h>

#include <vector>
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/latency/latency_info.h"

#if defined(OS_ANDROID)
#include "components/viz/common/quads/selection.h"
#include "ui/gfx/selection_bound.h"
#endif  // defined(OS_ANDROID)

namespace viz {

// Compares two frame tokens, handling cases where the token wraps around the
// 32-bit max value.
inline bool FrameTokenGT(uint32_t token1, uint32_t token2) {
  // There will be underflow in the subtraction if token1 was created
  // after token2.
  return (token2 - token1) > 0x80000000u;
}

class VIZ_COMMON_EXPORT FrameTokenGenerator {
 public:
  inline uint32_t operator++() {
    if (++frame_token_ == 0)
      ++frame_token_;
    return frame_token_;
  }

  inline uint32_t operator*() const { return frame_token_; }

 private:
  uint32_t frame_token_ = 0;
};

class VIZ_COMMON_EXPORT CompositorFrameMetadata {
 public:
  CompositorFrameMetadata();
  CompositorFrameMetadata(CompositorFrameMetadata&& other);
  ~CompositorFrameMetadata();

  CompositorFrameMetadata& operator=(CompositorFrameMetadata&& other);

  CompositorFrameMetadata Clone() const;

  // The device scale factor used to generate this compositor frame. Must be
  // greater than zero.
  float device_scale_factor = 0.f;

  // Scroll offset and scale of the root layer. This can be used for tasks
  // like positioning windowed plugins.
  gfx::Vector2dF root_scroll_offset;
  float page_scale_factor = 0.f;

  gfx::SizeF scrollable_viewport_size;

  bool may_contain_video = false;

  // WebView makes quality decisions for rastering resourceless software frames
  // based on information that a scroll or animation is active.
  // TODO(aelias): Remove this and always enable filtering if there aren't apps
  // depending on this anymore.
  bool is_resourceless_software_draw_with_scroll_or_animation = false;

  // This color is usually obtained from the background color of the <body>
  // element. It can be used for filling in gutter areas around the frame when
  // it's too small to fill the box the parent reserved for it.
  SkColor root_background_color = SK_ColorWHITE;

  std::vector<ui::LatencyInfo> latency_info;

  // This is the set of surfaces that the client wants to keep alive. It is
  // guaranteed that the last activated surface in every SurfaceRange will be
  // kept alive as long as the surface containing this CompositorFrame is alive.
  // Note: Not every surface in this list might have a corresponding
  // SurfaceDrawQuad, as this list also includes occluded and clipped surfaces
  // and surfaces that may be accessed by this CompositorFrame in the future.
  // However, every SurfaceDrawQuad MUST have a corresponding entry in this
  // list.
  std::vector<SurfaceRange> referenced_surfaces;

  // This is the set of dependent SurfaceIds that should be active in the
  // display compositor before this CompositorFrame can be activated.
  // Note: |activation_dependencies| MUST be a subset of |referenced_surfaces|.
  // TODO(samans): Rather than having a separate list for activation
  // dependencies, each member of referenced_surfaces can have a boolean flag
  // that determines whether activation of this particular SurfaceId blocks the
  // activation of the CompositorFrame. https://crbug.com/938946
  std::vector<SurfaceId> activation_dependencies;

  // This specifies a deadline for this CompositorFrame to synchronize with its
  // activation dependencies. Once this deadline passes, this CompositorFrame
  // should be forcibly activated. This deadline may be lower-bounded by the
  // default synchronization deadline specified by the system.
  FrameDeadline deadline;

  // BeginFrameAck for the BeginFrame that this CompositorFrame answers.
  BeginFrameAck begin_frame_ack;

  // An identifier for the frame. This is used to identify the frame for
  // presentation-feedback, or when the frame-token is sent to the embedder.
  // For comparing |frame_token| from different frames, use |FrameTokenGT()|
  // instead of directly comparing them, since the tokens wrap around back to 1
  // after the 32-bit max value.
  // TODO(crbug.com/850386): A custom type would be better to avoid incorrect
  // comparisons.
  uint32_t frame_token = 0;

  // Once the display compositor processes a frame with
  // |send_frame_token_to_embedder| flag turned on, the |frame_token| for the
  // frame is sent to embedder of the frame. This is helpful when the embedder
  // wants to do something after a particular frame is processed.
  bool send_frame_token_to_embedder = false;

  // These limits can be used together with the scroll/scale fields above to
  // determine if scrolling/scaling in a particular direction is possible.
  float min_page_scale_factor = 0.f;

  // Used to position the location top bar and page content, whose precise
  // position is computed by the renderer compositor.
  float top_controls_height = 0.f;
  float top_controls_shown_ratio = 0.f;

#if defined(OS_ANDROID)
  // Used to position Android bottom bar, whose position is computed by the
  // renderer compositor.
  float bottom_controls_height = 0.f;
  float bottom_controls_shown_ratio = 0.f;
#endif

  // The time at which the LocalSurfaceId used to submit this CompositorFrame
  // was allocated.
  base::TimeTicks local_surface_id_allocation_time;

  base::Optional<base::TimeDelta> preferred_frame_interval;

 private:
  CompositorFrameMetadata(const CompositorFrameMetadata& other);
  CompositorFrameMetadata operator=(const CompositorFrameMetadata&) = delete;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_METADATA_H_
