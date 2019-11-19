// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/video_types.h"

namespace viz {
class DisplayResourceProvider;
class RendererSettings;

// Holds all information necessary to construct a DCLayer from a DrawQuad.
class VIZ_SERVICE_EXPORT DCLayerOverlay {
 public:
  DCLayerOverlay();
  DCLayerOverlay(const DCLayerOverlay& other);
  DCLayerOverlay& operator=(const DCLayerOverlay& other);
  ~DCLayerOverlay();

  // TODO(magchen): Once software protected video is enabled for all GPUs and
  // all configurations, RequiresOverlay() will be true for all protected video.
  // Currently, we only force the overlay swap chain path (RequiresOverlay) for
  // hardware protected video and soon for Finch experiment on software
  // protected video.
  bool RequiresOverlay() const {
    return (protected_video_type ==
            gfx::ProtectedVideoType::kHardwareProtected);
  }

  // Resource ids for video Y and UV planes, a single NV12 image, or a swap
  // chain image. See DirectCompositionSurfaceWin for details.
  enum : size_t { kNumResources = 2 };
  ResourceId resources[kNumResources] = {kInvalidResourceId};

  // Mailboxes corresponding to |resources|. This is populated in SkiaRenderer
  // for accessing the textures on the GPU thread.
  gpu::Mailbox mailbox[kNumResources];

  // Stacking order relative to backbuffer which has z-order 0.
  int z_order = 1;

  // What part of the content to display in pixels.
  gfx::Rect content_rect;

  // Bounds of the overlay in pre-transform space.
  gfx::Rect quad_rect;

  // 2D flattened transform that maps |quad_rect| to root target space,
  // after applying the |quad_rect.origin()| as an offset.
  gfx::Transform transform;

  // If |is_clipped| is true, then clip to |clip_rect| in root target space.
  bool is_clipped = false;
  gfx::Rect clip_rect;

  // This is the color-space the texture should be displayed as. If invalid,
  // then the default for the texture should be used. For YUV textures, that's
  // normally BT.709.
  gfx::ColorSpace color_space;

  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
};

typedef std::vector<DCLayerOverlay> DCLayerOverlayList;

class VIZ_SERVICE_EXPORT DCLayerOverlayProcessor {
 public:
  DCLayerOverlayProcessor(const OutputSurface::Capabilities& capabilities,
                          const RendererSettings& settings);
  // For testing.
  DCLayerOverlayProcessor();
  ~DCLayerOverlayProcessor();

  void Process(DisplayResourceProvider* resource_provider,
               const gfx::RectF& display_rect,
               RenderPassList* render_passes,
               gfx::Rect* damage_rect,
               DCLayerOverlayList* dc_layer_overlays);
  void ClearOverlayState();
  // This is the damage contribution due to previous frame's overlays which can
  // be empty.
  gfx::Rect previous_frame_overlay_damage_contribution() {
    return previous_frame_overlay_rect_union_;
  }

 private:
  // Returns an iterator to the element after |it|.
  QuadList::Iterator ProcessRenderPassDrawQuad(RenderPass* render_pass,
                                               gfx::Rect* damage_rect,
                                               QuadList::Iterator it);
  void ProcessRenderPass(DisplayResourceProvider* resource_provider,
                         const gfx::RectF& display_rect,
                         RenderPass* render_pass,
                         bool is_root,
                         gfx::Rect* damage_rect,
                         DCLayerOverlayList* dc_layer_overlays);
  // Returns an iterator to the element after |it|.
  QuadList::Iterator ProcessForOverlay(const gfx::RectF& display_rect,
                                       RenderPass* render_pass,
                                       const gfx::Rect& quad_rectangle,
                                       const QuadList::Iterator& it,
                                       gfx::Rect* damage_rect);
  void ProcessForUnderlay(const gfx::RectF& display_rect,
                          RenderPass* render_pass,
                          const gfx::Rect& quad_rectangle,
                          const QuadList::Iterator& it,
                          bool is_root,
                          gfx::Rect* damage_rect,
                          gfx::Rect* this_frame_underlay_rect,
                          DCLayerOverlay* dc_layer);

  void InsertDebugBorderDrawQuads(const gfx::RectF& display_rect,
                                  const gfx::Rect& overlay_rect,
                                  RenderPass* root_render_pass,
                                  gfx::Rect* damage_rect);

  const bool has_hw_overlay_support_;
  const bool show_debug_borders_;

  gfx::Rect previous_frame_underlay_rect_;
  gfx::RectF previous_display_rect_;
  // previous and current overlay_rect_union_ include both overlay and underlay
  gfx::Rect previous_frame_overlay_rect_union_;
  gfx::Rect current_frame_overlay_rect_union_;
  int previous_frame_processed_overlay_count_ = 0;
  int current_frame_processed_overlay_count_ = 0;

  struct RenderPassData {
    RenderPassData();
    RenderPassData(const RenderPassData& other);
    ~RenderPassData();

    // Store information about clipped punch-through rects in target space for
    // non-root render passes. These rects are used to clear the corresponding
    // areas in parent render passes.
    std::vector<gfx::Rect> punch_through_rects;

    // Output rects of child render passes that have backdrop filters in target
    // space. These rects are used to determine if the overlay rect could be
    // read by backdrop filters.
    std::vector<gfx::Rect> backdrop_filter_rects;

    // Whether this render pass has backdrop filters.
    bool has_backdrop_filters = false;
  };
  base::flat_map<RenderPassId, RenderPassData> render_pass_data_;

  DISALLOW_COPY_AND_ASSIGN(DCLayerOverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
