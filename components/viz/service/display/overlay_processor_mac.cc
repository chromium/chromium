// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_mac.h"

#include <utility>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
OverlayProcessorMac::OverlayProcessorMac(bool enable_ca_overlay)
    : enable_ca_overlay_(enable_ca_overlay),
      ca_layer_overlay_processor_(std::make_unique<CALayerOverlayProcessor>()) {
}

OverlayProcessorMac::OverlayProcessorMac(
    std::unique_ptr<CALayerOverlayProcessor> ca_layer_overlay_processor)
    : enable_ca_overlay_(true),
      ca_layer_overlay_processor_(std::move(ca_layer_overlay_processor)) {}

OverlayProcessorMac::~OverlayProcessorMac() = default;

bool OverlayProcessorMac::DisableSplittingQuads() const {
  return true;
}

bool OverlayProcessorMac::IsOverlaySupported() const {
  return true;
}

gfx::Rect OverlayProcessorMac::GetPreviousFrameOverlaysBoundingRect() const {
  // This function's return value is used to determine the range of quads
  // produced by surface aggregation. We use the quads to generate our CALayer
  // tree every frame, and we use the quads that didn't change. For that
  // reason, always return the full frame.
  return previous_frame_full_bounding_rect_;
}

gfx::Rect OverlayProcessorMac::GetAndResetOverlayDamage() {
  gfx::Rect result = ca_overlay_damage_rect_;
  ca_overlay_damage_rect_ = gfx::Rect();
  return result;
}

void OverlayProcessorMac::ProcessForOverlays(
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
  TRACE_EVENT0("viz", "OverlayProcessorMac::ProcessForOverlays");
  auto& render_pass = render_passes->back();

  // Clear to get ready to handle output surface as overlay.
  output_surface_already_handled_ = false;
  previous_frame_full_bounding_rect_ = render_pass->output_rect;

  // Skip overlay processing if we have copy request.
  if (!render_pass->copy_requests.empty())
    return;

  // We could have surfaceless overlay but not ca overlay system on. In this
  // case we would still have the OutputSurfaceOverlayPlane.
  if (!enable_ca_overlay_)
    return;

  // First, try to use ProcessForCALayerOverlays to replace all DrawQuads in
  // |render_pass->quad_list| with CALayerOverlays in |candidates|.
  if (ca_layer_overlay_processor_->ProcessForCALayerOverlays(
          resource_provider, gfx::RectF(render_pass->output_rect),
          render_pass->quad_list, render_pass_filters,
          render_pass_backdrop_filters, candidates)) {
    // Mark the output surface as already handled (there is no output surface
    // anymore).
    output_surface_already_handled_ = true;

    // Set |last_overlay_damage_| to be everything, so that the next
    // frame that we draw to the output surface will do a full re-draw.
    ca_overlay_damage_rect_ = render_pass->output_rect;
    previous_frame_full_bounding_rect_ = ca_overlay_damage_rect_;

    // Everything in |render_pass->quad_list| has been moved over to
    // |candidates|. Ideally we would clear |render_pass->quad_list|, but some
    // RenderPass overlays still point into that list. So instead, to avoid
    // drawing the root RenderPass, we set |damage_rect| to be empty.
    *damage_rect = gfx::Rect();
  }

  // TODO(https://crbug.com/1152849): If there is any HDR or protected content,
  // use an underlay strategy to move those to |candidates| and replace them
  // with transparent quads in |render_pass->quad_list|.
}

void OverlayProcessorMac::AdjustOutputSurfaceOverlay(
    base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane->has_value())
    return;

  if (output_surface_already_handled_)
    output_surface_plane->reset();
}

bool OverlayProcessorMac::NeedsSurfaceDamageRectList() const {
  return false;
}

}  // namespace viz
