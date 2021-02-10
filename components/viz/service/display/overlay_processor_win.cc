// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_win.h"

#include <utility>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_utils.h"

namespace viz {
namespace {
// Switching between enabling DC layers and not is expensive, so only
// switch away after a large number of frames not needing DC layers have
// been produced.
constexpr int kNumberOfFramesBeforeDisablingDCLayers = 60;
}  // anonymous namespace

OverlayProcessorWin::OverlayProcessorWin(
    OutputSurface* output_surface,
    std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor)
    : output_surface_(output_surface),
      dc_layer_overlay_processor_(std::move(dc_layer_overlay_processor)) {
  DCHECK(output_surface_->capabilities().supports_dc_layers);
}

OverlayProcessorWin::~OverlayProcessorWin() = default;

bool OverlayProcessorWin::IsOverlaySupported() const {
  return true;
}

gfx::Rect OverlayProcessorWin::GetPreviousFrameOverlaysBoundingRect() const {
  // TODO(dcastagna): Implement me.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect OverlayProcessorWin::GetAndResetOverlayDamage() {
  return gfx::Rect();
}

void OverlayProcessorWin::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessorWin::ProcessForOverlays");

  auto* root_render_pass = render_passes->back().get();
  // Skip overlay processing if we have copy request.
  if (!root_render_pass->copy_requests.empty()) {
    damage_rect->Union(dc_layer_overlay_processor_
                           ->previous_frame_overlay_damage_contribution());
    // Update damage rect before calling ClearOverlayState, otherwise
    // previous_frame_overlay_rect_union will be empty.
    dc_layer_overlay_processor_->ClearOverlayState();
    return;
  }

  // Skip overlay processing if output colorspace is HDR.
  // Since most of overlay only supports NV12 and YUY2 now, HDR content (usually
  // P010 format) cannot output through overlay without format degrading. In
  // some Intel's platforms (Icelake or above), Overlay can play HDR content by
  // supporting RGB10 format. Let overlay deal with HDR content in this
  // situation.
  bool supports_rgb10a2_overlay =
      (gl::GetOverlaySupportFlags(DXGI_FORMAT_R10G10B10A2_UNORM) != 0 ||
       output_surface_->capabilities().forces_rgb10a2_overlay_support_flags);
  if (root_render_pass->content_color_usage == gfx::ContentColorUsage::kHDR &&
      !supports_rgb10a2_overlay) {
    return;
  }

  dc_layer_overlay_processor_->Process(
      resource_provider, gfx::RectF(root_render_pass->output_rect),
      render_pass_filters, render_pass_backdrop_filters, render_passes,
      damage_rect, std::move(surface_damage_rect_list), candidates);

  bool was_using_dc_layers = using_dc_layers_;
  if (!candidates->empty()) {
    using_dc_layers_ = true;
    frames_since_using_dc_layers_ = 0;
  } else if (++frames_since_using_dc_layers_ >=
             kNumberOfFramesBeforeDisablingDCLayers) {
    using_dc_layers_ = false;
  }

  if (was_using_dc_layers != using_dc_layers_) {
    output_surface_->SetEnableDCLayers(using_dc_layers_);
    // The entire surface has to be redrawn if switching from or to direct
    // composition layers, because the previous contents are discarded and some
    // contents would otherwise be undefined.
    *damage_rect = root_render_pass->output_rect;
  }
}

bool OverlayProcessorWin::NeedsSurfaceDamageRectList() const {
  return true;
}

}  // namespace viz
