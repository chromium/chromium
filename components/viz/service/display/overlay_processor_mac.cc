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
OverlayProcessorMac::OverlayProcessorMac(bool could_overlay,
                                         bool enable_ca_overlay)
    : could_overlay_(could_overlay),
      enable_ca_overlay_(enable_ca_overlay),
      ca_layer_overlay_processor_(std::make_unique<CALayerOverlayProcessor>()) {
}

OverlayProcessorMac::OverlayProcessorMac(
    std::unique_ptr<CALayerOverlayProcessor> ca_layer_overlay_processor)
    : could_overlay_(true),
      enable_ca_overlay_(true),
      ca_layer_overlay_processor_(std::move(ca_layer_overlay_processor)) {}

OverlayProcessorMac::~OverlayProcessorMac() = default;

bool OverlayProcessorMac::DisableSplittingQuads() const {
  return true;
}

bool OverlayProcessorMac::IsOverlaySupported() const {
  return could_overlay_;
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

  // If ca overlay system didn't succeed, we fall back to surfaceless.
  if (!ca_layer_overlay_processor_->ProcessForCALayerOverlays(
          resource_provider, gfx::RectF(render_pass->output_rect),
          render_pass->quad_list, render_pass_filters,
          render_pass_backdrop_filters, candidates))
    return;

  // CALayer overlays are all-or-nothing. If all quads were replaced with
  // layers then mark the output surface as already handled.
  output_surface_already_handled_ = true;
  ca_overlay_damage_rect_ = render_pass->output_rect;
  previous_frame_full_bounding_rect_ = ca_overlay_damage_rect_;
  *damage_rect = gfx::Rect();
}

void OverlayProcessorMac::AdjustOutputSurfaceOverlay(
    base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane->has_value())
    return;

  if (output_surface_already_handled_)
    output_surface_plane->reset();
}

bool OverlayProcessorMac::NeedsSurfaceOccludingDamageRect() const {
  return false;
}

}  // namespace viz
