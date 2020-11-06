// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_using_strategy.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace viz {

// Default implementation of whether a strategy would remove the output surface
// as overlay plane.
bool OverlayProcessorUsingStrategy::Strategy::RemoveOutputSurfaceAsOverlay() {
  return false;
}

OverlayStrategy OverlayProcessorUsingStrategy::Strategy::GetUMAEnum() const {
  return OverlayStrategy::kUnknown;
}

OverlayProcessorUsingStrategy::OverlayProcessorUsingStrategy()
    : OverlayProcessorInterface() {}

OverlayProcessorUsingStrategy::~OverlayProcessorUsingStrategy() = default;

gfx::Rect OverlayProcessorUsingStrategy::GetPreviousFrameOverlaysBoundingRect()
    const {
  gfx::Rect result = overlay_damage_rect_;
  result.Union(previous_frame_underlay_rect_);
  return result;
}

gfx::Rect OverlayProcessorUsingStrategy::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

void OverlayProcessorUsingStrategy::NotifyOverlayPromotion(
    DisplayResourceProvider* display_resource_provider,
    const CandidateList& candidates,
    const QuadList& quad_list) {}

void OverlayProcessorUsingStrategy::ProcessForOverlays(
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
  TRACE_EVENT0("viz", "OverlayProcessorUsingStrategy::ProcessForOverlays");
  DCHECK(candidates->empty());

  auto* render_pass = render_passes->back().get();

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (!render_pass->copy_requests.empty()) {
    // Reset |previous_frame_underlay_rect_| in case UpdateDamageRect() not
    // being invoked.  Also reset |previous_frame_underlay_was_unoccluded_|.
    previous_frame_underlay_rect_ = gfx::Rect();
    previous_frame_underlay_was_unoccluded_ = false;
    NotifyOverlayPromotion(resource_provider, *candidates,
                           render_pass->quad_list);
    return;
  }

  // Only if that fails, attempt hardware overlay strategies.
  bool success = AttemptWithStrategies(
      output_color_matrix, render_pass_backdrop_filters, resource_provider,
      render_passes, output_surface_plane, candidates, content_bounds);

  if (success) {
    UpdateDamageRect(candidates, previous_frame_underlay_rect_,
                     previous_frame_underlay_was_unoccluded_,
                     &render_pass->quad_list, damage_rect);
  } else {
    if (!previous_frame_underlay_rect_.IsEmpty())
      damage_rect->Union(previous_frame_underlay_rect_);

    DCHECK(candidates->empty());

    previous_frame_underlay_rect_ = gfx::Rect();
    previous_frame_underlay_was_unoccluded_ = false;
  }

  NotifyOverlayPromotion(resource_provider, *candidates,
                         render_pass->quad_list);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

// Subtract on-top opaque overlays from the damage rect, unless the overlays
// use the backbuffer as their content (in which case, add their combined rect
// back to the damage at the end).
// Also subtract unoccluded underlays from the damage rect if we know that the
// same underlay was scheduled on the previous frame. If the renderer decides
// not to swap the framebuffer there will still be a transparent hole in the
// previous frame.
void OverlayProcessorUsingStrategy::UpdateDamageRect(
    OverlayCandidateList* candidates,
    const gfx::Rect& previous_frame_underlay_rect,
    bool previous_frame_underlay_was_unoccluded,
    const QuadList* quad_list,
    gfx::Rect* damage_rect) {
  gfx::Rect this_frame_underlay_rect;
  for (const OverlayCandidate& overlay : *candidates) {
    if (overlay.plane_z_order >= 0) {
      const gfx::Rect overlay_display_rect =
          GetOverlayDamageRectForOutputSurface(overlay);
      // If an overlay candidate comes from output surface, its z-order should
      // be 0.
      overlay_damage_rect_.Union(overlay_display_rect);
      if (overlay.is_opaque)
        damage_rect->Subtract(overlay_display_rect);
    } else {
      // Process underlay candidates:
      // Track the underlay_rect from frame to frame. If it is the same and
      // nothing is on top of it then that rect doesn't need to be damaged
      // because the drawing is occurring on a different plane. If it is
      // different then that indicates that a different underlay has been
      // chosen and the previous underlay rect should be damaged because it
      // has changed planes from the underlay plane to the main plane. It then
      // checks that this is not a transition from occluded to unoccluded.
      //
      // We also insist that the underlay is unoccluded for at least one frame,
      // else when content above the overlay transitions from not fully
      // transparent to fully transparent, we still need to erase it from the
      // framebuffer.  Otherwise, the last non-transparent frame will remain.
      // https://crbug.com/875879
      // However, if the underlay is unoccluded, we check if the damage is due
      // to a solid-opaque-transparent quad. If so, then we subtract this
      // damage.
      this_frame_underlay_rect = GetOverlayDamageRectForOutputSurface(overlay);

      bool same_underlay_rect =
          this_frame_underlay_rect == previous_frame_underlay_rect;
      bool transition_from_occluded_to_unoccluded =
          overlay.is_unoccluded && !previous_frame_underlay_was_unoccluded;
      bool always_unoccluded =
          overlay.is_unoccluded && previous_frame_underlay_was_unoccluded;

      // We need to make sure that when we change the overlay we damage the
      // region where the underlay will be positioned. This is because a
      // black transparent hole is made for the underlay to show through
      // but its possible that the damage for this quad is less than the
      // complete size of the underlay.  https://crbug.com/1130733
      if (!same_underlay_rect) {
        damage_rect->Union(this_frame_underlay_rect);
      }

      if (same_underlay_rect && !transition_from_occluded_to_unoccluded &&
          (always_unoccluded || overlay.no_occluding_damage)) {
        damage_rect->Subtract(this_frame_underlay_rect);
      }
      previous_frame_underlay_was_unoccluded_ = overlay.is_unoccluded;
    }

    if (overlay.plane_z_order) {
      RecordOverlayDamageRectHistograms(
          (overlay.plane_z_order > 0), !overlay.no_occluding_damage,
          damage_rect->IsEmpty(),
          false /* occluding_damage_equal_to_damage_rect */);
    }
  }

  if (this_frame_underlay_rect != previous_frame_underlay_rect)
    damage_rect->Union(previous_frame_underlay_rect);

  previous_frame_underlay_rect_ = this_frame_underlay_rect;
}

void OverlayProcessorUsingStrategy::AdjustOutputSurfaceOverlay(
    base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane || !output_surface_plane->has_value())
    return;

  // If the overlay candidates cover the entire screen, the
  // |output_surface_plane| could be removed.
  if (last_successful_strategy_ &&
      last_successful_strategy_->RemoveOutputSurfaceAsOverlay())
    output_surface_plane->reset();
}

bool OverlayProcessorUsingStrategy::AttemptWithStrategies(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  last_successful_strategy_ = nullptr;
  for (const auto& strategy : strategies_) {
    if (strategy->Attempt(output_color_matrix, render_pass_backdrop_filters,
                          resource_provider, render_pass_list, primary_plane,
                          candidates, content_bounds)) {
      // This function is used by underlay strategy to mark the primary plane as
      // enable_blending.
      strategy->AdjustOutputSurfaceOverlay(primary_plane);
      UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                                strategy->GetUMAEnum());
      last_successful_strategy_ = strategy.get();
      return true;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                            OverlayStrategy::kNoStrategyUsed);
  return false;
}

gfx::Rect OverlayProcessorUsingStrategy::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}
}  // namespace viz
