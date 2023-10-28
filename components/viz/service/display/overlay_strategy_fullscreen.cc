// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_fullscreen.h"

#include <vector>

#include "components/viz/common/features.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace viz {

OverlayStrategyFullscreen::OverlayStrategyFullscreen(
    OverlayProcessorUsingStrategy* capability_checker)
    : capability_checker_(capability_checker) {
  DCHECK(capability_checker);
}

OverlayStrategyFullscreen::~OverlayStrategyFullscreen() {}

void OverlayStrategyFullscreen::Propose(
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    std::vector<OverlayProposedCandidate>* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  auto* render_pass = render_pass_list->back().get();
  QuadList* quad_list = &render_pass->quad_list;
  // First non invisible quad of quad_list is the top most quad.
  auto front = quad_list->begin();
  while (front != quad_list->end()) {
    if (!OverlayCandidate::IsInvisibleQuad(*front))
      break;
    ++front;
  }

  if (front == quad_list->end())
    return;

  const DrawQuad* quad = *front;
  if (quad->ShouldDrawWithBlendingForReasonOtherThanMaskFilter()) {
    return;
  }

  OverlayCandidate candidate;
  const OverlayCandidateFactory::OverlayContext context = {
      .supports_mask_filter = false};

  OverlayCandidateFactory candidate_factory = OverlayCandidateFactory(
      render_pass, resource_provider, surface_damage_rect_list,
      &output_color_matrix, GetPrimaryPlaneDisplayRect(primary_plane),
      &render_pass_filters, context);
  if (candidate_factory.FromDrawQuad(quad, candidate) !=
      OverlayCandidate::CandidateStatus::kSuccess) {
    return;
  }

  if (!candidate.display_rect.origin().IsOrigin() ||
      gfx::ToRoundedSize(candidate.display_rect.size()) !=
          render_pass->output_rect.size()) {
    // Candidate Quad does not fully cover display but fullscreen is still
    // possible if all the other quads do not contribute to primary plane or
    // their contribution will simply result in the default black of DRM. The
    // best example here is the black bars for aspect ratio found in fullscreen
    // video.
    if (!base::FeatureList::IsEnabled(
            features::kUseDrmBlackFullscreenOptimization)) {
      return;
    }

    auto after_front = front;
    ++after_front;
    while (after_front != quad_list->end()) {
      if (!(*after_front)->visible_rect.IsEmpty() &&
          !OverlayCandidate::IsInvisibleQuad(*after_front)) {
        auto* solid_color_quad =
            (*after_front)->DynamicCast<SolidColorDrawQuad>();
        if (!solid_color_quad) {
          return;
        }

        if (solid_color_quad->color != SkColors::kBlack) {
          return;
        }
      }
      ++after_front;
    }
  }

  candidate.is_opaque = true;
  candidate.plane_z_order = 0;
  candidates->emplace_back(front, candidate, this);
}

bool OverlayStrategyFullscreen::Attempt(
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds,
    const OverlayProposedCandidate& proposed_candidate) {
  // Before we attempt an overlay strategy, the candidate list should be empty.
  DCHECK(candidate_list->empty());

  OverlayCandidateList new_candidate_list;
  new_candidate_list.push_back(proposed_candidate.candidate);
  capability_checker_->CheckOverlaySupport(nullptr, &new_candidate_list);
  if (!new_candidate_list.front().overlay_handled)
    return false;

  candidate_list->swap(new_candidate_list);
  auto* render_pass = render_pass_list->back().get();
  CommitCandidate(proposed_candidate, render_pass);
  return true;
}

void OverlayStrategyFullscreen::CommitCandidate(
    const OverlayProposedCandidate& proposed_candidate,
    AggregatedRenderPass* render_pass) {
  render_pass->quad_list = QuadList();  // Remove all the quads
}

OverlayStrategy OverlayStrategyFullscreen::GetUMAEnum() const {
  return OverlayStrategy::kFullscreen;
}

bool OverlayStrategyFullscreen::RemoveOutputSurfaceAsOverlay() {
  // This is called when the strategy is successful. In this case the entire
  // screen is covered by the overlay candidate and there is no need to overlay
  // the output surface.
  return true;
}

}  // namespace viz
