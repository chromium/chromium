// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_METADATA_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_METADATA_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/latency/latency_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/viz/common/quads/selection.h"
#include "ui/gfx/selection_bound.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace viz {

// A frame token value of 0 indicates an invalid or local frame token. A
// local frame token is used inside viz when it creates its own CompositorFrame
// for a surface.
inline constexpr uint32_t kInvalidOrLocalFrameToken = 0;

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
    if (++frame_token_ == kInvalidOrLocalFrameToken) {
      ++frame_token_;
    }
    return frame_token_;
  }

  inline uint32_t operator*() const { return frame_token_; }

 private:
  uint32_t frame_token_ = kInvalidOrLocalFrameToken;
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
  gfx::PointF root_scroll_offset;
  float page_scale_factor = 0.f;

  gfx::SizeF scrollable_viewport_size;

  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kSRGB;

  bool may_contain_video = false;

  bool may_throttle_if_undrawn_frames = true;

  // WebView makes quality decisions for rastering resourceless software frames
  // based on information that a scroll or animation is active.
  // TODO(aelias): Remove this and always enable filtering if there aren't apps
  // depending on this anymore.
  bool is_resourceless_software_draw_with_scroll_or_animation = false;

  // True if this compositor frame is related to an animated or precise scroll.
  // This includes during the touch interaction just prior to the initiation of
  // gesture scroll events.
  bool is_handling_interaction = false;

  // This color is usually obtained from the background color of the <body>
  // element. It can be used for filling in gutter areas around the frame when
  // it's too small to fill the box the parent reserved for it.
  SkColor4f root_background_color = SkColors::kWhite;

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
  uint32_t frame_token = kInvalidOrLocalFrameToken;

  // Once the display compositor processes a frame with
  // |send_frame_token_to_embedder| flag turned on, the |frame_token| for the
  // frame is sent to embedder of the frame. This is helpful when the embedder
  // wants to do something after a particular frame is processed.
  bool send_frame_token_to_embedder = false;

  // These limits can be used together with the scroll/scale fields above to
  // determine if scrolling/scaling in a particular direction is possible.
  float min_page_scale_factor = 0.f;

  // The visible height of the top-controls. If the value is not set, then the
  // visible height should be the same as in the latest submitted frame with a
  // value set.
  absl::optional<float> top_controls_visible_height;

  absl::optional<base::TimeDelta> preferred_frame_interval;

  // Display transform hint when the frame is generated. Note this is only
  // applicable to frames of the root surface.
  gfx::OverlayTransform display_transform_hint = gfx::OVERLAY_TRANSFORM_NONE;

  // Contains the metadata required for drawing a delegated ink trail onto the
  // end of a rendered ink stroke. This should only be present when two
  // conditions are met:
  //   1. The JS API |updateInkTrailStartPoint| is used - This gathers the
  //     metadata and puts it onto a compositor frame to be sent to viz.
  //   2. This frame will not be submitted to the root surface - The browser UI
  //     does not use this, and the frame must be contained within a
  //     SurfaceDrawQuad.
  // This metadata will be copied when an aggregated frame is made, and will be
  // used until this Compositor Frame Metadata is replaced.
  std::unique_ptr<gfx::DelegatedInkMetadata> delegated_ink_metadata;

  // This represents a list of directives to execute in order to support the
  // view transitions.
  std::vector<CompositorFrameTransitionDirective> transition_directives;

  // A map of region capture crop ids associated with this frame to the
  // gfx::Rect of the region that they represent.
  RegionCaptureBounds capture_bounds;

  // Indicates if this frame references shared element resources that need to
  // be replaced with ResourceIds in the Viz process.
  bool has_shared_element_resources = false;

 private:
  CompositorFrameMetadata(const CompositorFrameMetadata& other);
  CompositorFrameMetadata operator=(const CompositorFrameMetadata&) = delete;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_METADATA_H_
