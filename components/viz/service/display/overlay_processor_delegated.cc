// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_delegated.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/transform.h"

#include "components/viz/common/quads/texture_draw_quad.h"

namespace viz {

OverlayProcessorDelegated::OverlayProcessorDelegated(
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
    std::vector<OverlayStrategy> available_strategies,
    gpu::SharedImageInterface* shared_image_interface)
    : OverlayProcessorOzone(std::move(overlay_candidates),
                            available_strategies,
                            shared_image_interface) {}

OverlayProcessorDelegated::~OverlayProcessorDelegated() = default;

bool OverlayProcessorDelegated::AttemptWithStrategies(
    const skia::Matrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  DCHECK(candidates->empty());

  auto* render_pass = render_pass_list->back().get();
  QuadList* quad_list = &render_pass->quad_list;
  const bool allow_delegated_quads = true;

  // Wayland overlay forwarding mechanism does not support putting the same
  // buffer on multiple surfaces. We use this |candidate_ids| for best effort
  // service and simply skip duplicate resource id quads.
  std::set<ResourceId> candidate_ids;
  std::vector<QuadList::Iterator> candidate_quads;
  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    OverlayCandidate candidate;
    auto& transform = it->shared_quad_state->quad_to_target_transform;
    auto display_rect = gfx::RectF(it->rect);
    transform.TransformRect(&display_rect);
    DBG_DRAW_TEXT(
        "delegated.overlay.type",
        gfx::Vector2dF(display_rect.origin().x(), display_rect.origin().y()),
        base::StringPrintf("m=%d rid=%d", static_cast<int>(it->material),
                           it->resources.begin()->value()));
    if (OverlayCandidate::FromDrawQuad(
            resource_provider, surface_damage_rect_list, output_color_matrix,
            *it, GetPrimaryPlaneDisplayRect(primary_plane), &candidate,
            allow_delegated_quads)) {
      // Setting the |uv_rect| will eventually result in setting the
      // |crop_rect_| in wayland. If this results in an empty pixel scale the
      // wayland connection will be terminated. See: wayland_surface.cc
      // 'UpdateBufferDamageRegion'
      auto viewport_src = gfx::ToEnclosedRect(gfx::ScaleRect(
          candidate.uv_rect, candidate.resource_size_in_pixels.width(),
          candidate.resource_size_in_pixels.height()));

      if (it->material == DrawQuad::Material::kSolidColor) {
        DBG_DRAW_RECT("delegated.overlay.color", candidate.display_rect);
        candidates->push_back(candidate);
        candidate_quads.push_back(it);
        continue;
      }

      if (it->material == DrawQuad::Material::kAggregatedRenderPass) {
        DBG_DRAW_RECT("delegated.overlay.aggregated", candidate.display_rect);
        candidates->push_back(candidate);
        candidate_quads.push_back(it);
        continue;
      }

      // Because of the device scale factor (2) we check against a rounded empty
      // rect.
      // TODO(https://crbug.com/1218678) : Move and generalize this fix in
      // wayland host.
      if (viewport_src.width() <= 2 || viewport_src.height() <= 2) {
        DBG_DRAW_RECT("delegated.overlay.subpixelskip", candidate.display_rect);
        continue;
      }

      if (candidate_ids.find(candidate.resource_id) == candidate_ids.end()) {
        candidate_ids.insert(candidate.resource_id);
        DBG_DRAW_RECT("delegated.overlay.candidate", candidate.display_rect);
        candidates->push_back(candidate);
        candidate_quads.push_back(it);
      } else {
        DBG_DRAW_RECT("delegated.overlay.dupskip", candidate.display_rect);
      }

    } else {
      DBG_DRAW_RECT("delegated.overlay.failed", display_rect);
    }
  }

  if (candidates->empty())
    return false;

  int curr_plane_order = candidates->size();
  for (auto&& each : *candidates) {
    each.plane_z_order = curr_plane_order--;
  }
  // Check for support.
  this->CheckOverlaySupport(primary_plane, candidates);

  // We cannot erase the quads that were handled as overlays because raw
  // pointers of the aggregate draw quads were placed in the |rpdq| member of
  // the |OverlayCandidate|. As keeping with the pattern in
  // overlay_processor_mac we will also set the damage to empty on the
  // successful promotion of all quads.

  return true;
}

gfx::RectF OverlayProcessorDelegated::GetPrimaryPlaneDisplayRect(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane) {
  return primary_plane ? primary_plane->display_rect : gfx::RectF();
}

void OverlayProcessorDelegated::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const skia::Matrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  DCHECK(candidates->empty());
  auto* render_pass = render_passes->back().get();
  bool success = false;

  DBG_DRAW_RECT("delegated.incoming.damage", (*damage_rect));
  for (auto&& each : surface_damage_rect_list) {
    DBG_DRAW_RECT("delegated.surface.damage", each);
  }

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (render_pass->copy_requests.empty()) {
    success = AttemptWithStrategies(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_passes, &surface_damage_rect_list, output_surface_plane,
        candidates, content_bounds);
  }

  DCHECK(candidates->empty() || success);
  // TODO(petermcneeley) : Actually modify the damage here. When all damaged
  // quads are forwarded the damage here will be empty.
  DBG_DRAW_RECT("delegated.outgoing.damage", (*damage_rect));

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

}  // namespace viz
