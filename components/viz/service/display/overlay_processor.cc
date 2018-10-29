// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace viz {

namespace {

#if defined(OS_ANDROID)
// Utility class to make sure that we notify resource that they're promotable
// before returning from ProcessForOverlays.
class SendPromotionHintsBeforeReturning {
 public:
  SendPromotionHintsBeforeReturning(DisplayResourceProvider* resource_provider,
                                    OverlayCandidateList* candidates)
      : resource_provider_(resource_provider), candidates_(candidates) {}
  ~SendPromotionHintsBeforeReturning() {
    resource_provider_->SendPromotionHints(
        candidates_->promotion_hint_info_map_);
  }

 private:
  DisplayResourceProvider* resource_provider_;
  OverlayCandidateList* candidates_;

  DISALLOW_COPY_AND_ASSIGN(SendPromotionHintsBeforeReturning);
};
#endif

}  // namespace

OverlayProcessor::StrategyType OverlayProcessor::Strategy::GetUMAEnum() const {
  return StrategyType::kUnknown;
}

OverlayProcessor::OverlayProcessor(OutputSurface* surface)
    : surface_(surface) {}

void OverlayProcessor::Initialize() {
  DCHECK(surface_);
  OverlayCandidateValidator* validator =
      surface_->GetOverlayCandidateValidator();
  if (validator)
    validator->GetStrategies(&strategies_);
}

OverlayProcessor::~OverlayProcessor() {}

gfx::Rect OverlayProcessor::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

bool OverlayProcessor::ProcessForCALayers(
    DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OverlayCandidateList* overlay_candidates,
    CALayerOverlayList* ca_layer_overlays,
    gfx::Rect* damage_rect) {
  OverlayCandidateValidator* overlay_validator =
      surface_->GetOverlayCandidateValidator();
  if (!overlay_validator || !overlay_validator->AllowCALayerOverlays())
    return false;

  if (!ProcessForCALayerOverlays(
          resource_provider, gfx::RectF(render_pass->output_rect),
          render_pass->quad_list, render_pass_filters,
          render_pass_backdrop_filters, ca_layer_overlays))
    return false;

  // CALayer overlays are all-or-nothing. If all quads were replaced with
  // layers then clear the list and remove the backbuffer from the overcandidate
  // list.
  overlay_candidates->clear();
  overlay_damage_rect_ = render_pass->output_rect;
  *damage_rect = gfx::Rect();
  return true;
}

bool OverlayProcessor::ProcessForDCLayers(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OverlayCandidateList* overlay_candidates,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect) {
  OverlayCandidateValidator* overlay_validator =
      surface_->GetOverlayCandidateValidator();
  if (!overlay_validator || !overlay_validator->AllowDCLayerOverlays())
    return false;

  dc_processor_.Process(
      resource_provider, gfx::RectF(render_passes->back()->output_rect),
      render_passes, &overlay_damage_rect_, damage_rect, dc_layer_overlays);

  DCHECK(overlay_candidates->empty());
  return true;
}

void OverlayProcessor::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OverlayCandidateList* candidates,
    CALayerOverlayList* ca_layer_overlays,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessor::ProcessForOverlays");
#if defined(OS_ANDROID)
  // Be sure to send out notifications, regardless of whether we get to
  // processing for overlays or not.  If we don't, then we should notify that
  // they are not promotable.
  SendPromotionHintsBeforeReturning notifier(resource_provider, candidates);
#endif

  // Reset |previous_frame_underlay_rect_| in case UpdateDamageRect() not being
  // invoked.  Also reset |previous_frame_underlay_was_unoccluded_|.
  const gfx::Rect previous_frame_underlay_rect = previous_frame_underlay_rect_;
  previous_frame_underlay_rect_ = gfx::Rect();
  bool previous_frame_underlay_was_unoccluded =
      previous_frame_underlay_was_unoccluded_;
  previous_frame_underlay_was_unoccluded_ = false;

  RenderPass* render_pass = render_passes->back().get();

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (!render_pass->copy_requests.empty()) {
    dc_processor_.ClearOverlayState();
    return;
  }

  // First attempt to process for CALayers.
  if (ProcessForCALayers(resource_provider, render_passes->back().get(),
                         render_pass_filters, render_pass_backdrop_filters,
                         candidates, ca_layer_overlays, damage_rect)) {
    return;
  }

  if (ProcessForDCLayers(resource_provider, render_passes, render_pass_filters,
                         render_pass_backdrop_filters, candidates,
                         dc_layer_overlays, damage_rect)) {
    return;
  }

  // Only if that fails, attempt hardware overlay strategies.
  Strategy* successful_strategy = nullptr;
  for (const auto& strategy : strategies_) {
    if (!strategy->Attempt(output_color_matrix, render_pass_backdrop_filters,
                           resource_provider, render_passes->back().get(),
                           candidates, content_bounds)) {
      continue;
    }
    successful_strategy = strategy.get();
    UpdateDamageRect(candidates, previous_frame_underlay_rect,
                     previous_frame_underlay_was_unoccluded, damage_rect);
    break;
  }

  if (!successful_strategy && !previous_frame_underlay_rect.IsEmpty())
    damage_rect->Union(previous_frame_underlay_rect);

  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                            successful_strategy
                                ? successful_strategy->GetUMAEnum()
                                : StrategyType::kNoStrategyUsed);

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

// Subtract on-top opaque overlays from the damage rect, unless the overlays use
// the backbuffer as their content (in which case, add their combined rect
// back to the damage at the end).
// Also subtract unoccluded underlays from the damage rect if we know that the
// same underlay was scheduled on the previous frame. If the renderer decides
// not to swap the framebuffer there will still be a transparent hole in the
// previous frame.
void OverlayProcessor::UpdateDamageRect(
    OverlayCandidateList* candidates,
    const gfx::Rect& previous_frame_underlay_rect,
    bool previous_frame_underlay_was_unoccluded,
    gfx::Rect* damage_rect) {
  gfx::Rect output_surface_overlay_damage_rect;
  gfx::Rect this_frame_underlay_rect;
  for (const OverlayCandidate& overlay : *candidates) {
    if (overlay.plane_z_order >= 0) {
      const gfx::Rect overlay_display_rect =
          ToEnclosedRect(overlay.display_rect);
      if (overlay.use_output_surface_for_resource) {
        if (overlay.plane_z_order > 0)
          output_surface_overlay_damage_rect.Union(overlay_display_rect);
      } else {
        overlay_damage_rect_.Union(overlay_display_rect);
        if (overlay.is_opaque)
          damage_rect->Subtract(overlay_display_rect);
      }
    } else if (this_frame_underlay_rect.IsEmpty()) {
      // Process underlay candidates:
      // Track the underlay_rect from frame to frame.  If it is the same
      // and nothing is on top of it then that rect doesn't need to
      // be damaged because the drawing is occurring on a different plane.
      // If it is different then that indicates that a different underlay
      // has been chosen and the previous underlay rect should be damaged
      // because it has changed planes from the underlay plane to the
      // main plane.
      //
      // We also insist that the underlay is unoccluded for at leat one frame,
      // else when content above the overlay transitions from not fully
      // transparent to fully transparent, we still need to erase it from the
      // framebuffer.  Otherwise, the last non-transparent frame will remain.
      // https://crbug.com/875879
      this_frame_underlay_rect = ToEnclosedRect(overlay.display_rect);
      if ((this_frame_underlay_rect == previous_frame_underlay_rect) &&
          overlay.is_unoccluded && previous_frame_underlay_was_unoccluded) {
        damage_rect->Subtract(this_frame_underlay_rect);
      }
      previous_frame_underlay_was_unoccluded_ = overlay.is_unoccluded;
    }
  }

  if (this_frame_underlay_rect != previous_frame_underlay_rect)
    damage_rect->Union(previous_frame_underlay_rect);

  previous_frame_underlay_rect_ = this_frame_underlay_rect;

  damage_rect->Union(output_surface_overlay_damage_rect);
}

namespace {

bool DiscardableQuad(const DrawQuad* q) {
  return q->material == DrawQuad::SOLID_COLOR &&
      (SolidColorDrawQuad::MaterialCast(q)->color == SK_ColorBLACK ||
       SolidColorDrawQuad::MaterialCast(q)->color == SK_ColorTRANSPARENT);
}

}

// static
void OverlayProcessor::EliminateOrCropPrimary(
    const QuadList& quad_list,
    const QuadList::Iterator& candidate_iterator,
    OverlayCandidate* primary,
    OverlayCandidateList* candidate_list) {
  gfx::RectF content_rect;

  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    if (it == candidate_iterator)
      continue;
    if (!DiscardableQuad(*it)) {
      auto& transform = it->shared_quad_state->quad_to_target_transform;
      gfx::RectF display_rect = gfx::RectF(it->rect);
      transform.TransformRect(&display_rect);
      content_rect.Union(display_rect);
    }
  }

  if (!content_rect.IsEmpty()) {
    // Sometimes the content quads extend past primary->display_rect, so first
    // clip the content_rect to that.
    content_rect.Intersect(primary->display_rect);
    DCHECK_NE(0, primary->display_rect.width());
    DCHECK_NE(0, primary->display_rect.height());
    primary->uv_rect =
        gfx::ScaleRect(content_rect, 1. / primary->display_rect.width(),
                       1. / primary->display_rect.height());
    primary->display_rect = content_rect;

    candidate_list->push_back(*primary);
  }
}


}  // namespace viz
