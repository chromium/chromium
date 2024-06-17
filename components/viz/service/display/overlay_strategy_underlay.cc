// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_underlay.h"

#include <vector>

#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_candidate_factory.h"

namespace viz {

OverlayStrategyUnderlay::OverlayStrategyUnderlay(
    OverlayProcessorUsingStrategy* capability_checker,
    OpaqueMode opaque_mode)
    : capability_checker_(capability_checker), opaque_mode_(opaque_mode) {
  DCHECK(capability_checker);
}

OverlayStrategyUnderlay::~OverlayStrategyUnderlay() {}

void OverlayStrategyUnderlay::Propose(
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    const DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    std::vector<OverlayProposedCandidate>* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  auto* render_pass = render_pass_list->back().get();
  QuadList& quad_list = render_pass->quad_list;

  OverlayCandidateFactory::OverlayContext context;
  context.supports_mask_filter = true;
  context.supports_flip_rotate_transform =
      capability_checker_->SupportsFlipRotateTransform();

  OverlayCandidateFactory candidate_factory = OverlayCandidateFactory(
      render_pass, resource_provider, surface_damage_rect_list,
      &output_color_matrix, GetPrimaryPlaneDisplayRect(primary_plane),
      &render_pass_filters, context);

  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    OverlayCandidate candidate;
    candidate.overlay_type = gfx::OverlayType::kUnderlay;
    if (candidate_factory.FromDrawQuad(*it, candidate) !=
            OverlayCandidate::CandidateStatus::kSuccess ||
        (opaque_mode_ == OpaqueMode::RequireOpaqueCandidates &&
         !candidate.is_opaque)) {
      continue;
    }

    // Filters read back the framebuffer to get the pixel values that need to
    // be filtered.  This is a problem when there are hardware planes because
    // the planes are not composited until they are on the display controller.
    // If we are requiring an overlay, then we should not block it due to this
    // condition.
    if (!candidate.requires_overlay &&
        candidate_factory.IsOccludedByFilteredQuad(
            candidate, quad_list.begin(), it, render_pass_backdrop_filters)) {
      continue;
    }

    candidate.damage_area_estimate = candidate_factory.EstimateVisibleDamage(
        *it, candidate, quad_list.begin(), it);

    candidates->emplace_back(it, candidate, this);
  }
}

bool OverlayStrategyUnderlay::Attempt(
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    const DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds,
    const OverlayProposedCandidate& proposed_candidate) {
  // Before we attempt an overlay strategy, the candidate list should be empty.
  DCHECK(candidate_list->empty());
  auto* render_pass = render_pass_list->back().get();

  // Add the overlay.
  OverlayCandidateList new_candidate_list = *candidate_list;
  new_candidate_list.push_back(proposed_candidate.candidate);
  new_candidate_list.back().plane_z_order = -1;

  if (primary_plane) {
    // Since there is a list of strategies to go through, each strategy should
    // not change the input parameters. In this case, we need to keep the
    // |primary_plane| unchanged. The underlay strategy only works when the
    // |primary_plane| supports blending. In order to check the hardware
    // support, make a copy of the |primary_plane| with blending enabled.
    PrimaryPlane new_plane_candidate(*primary_plane);
    new_plane_candidate.enable_blending = true;
    // Check for support.
    capability_checker_->CheckOverlaySupport(&new_plane_candidate,
                                             &new_candidate_list);
  } else {
    capability_checker_->CheckOverlaySupport(nullptr, &new_candidate_list);
  }

  if (new_candidate_list.back().overlay_handled) {
    CommitCandidate(proposed_candidate, render_pass);
    candidate_list->swap(new_candidate_list);
    return true;
  }

  return false;
}

void OverlayStrategyUnderlay::CommitCandidate(
    const OverlayProposedCandidate& proposed_candidate,
    AggregatedRenderPass* render_pass) {
  // If the candidate can be handled by an overlay, create a pass for it. We
  // need to switch out the video quad with an underlay hole quad.
  if (proposed_candidate.candidate.has_mask_filter) {
    render_pass->ReplaceExistingQuadWithSolidColor(
        proposed_candidate.quad_iter, SkColors::kBlack, SkBlendMode::kDstOut);
  } else {
    render_pass->ReplaceExistingQuadWithSolidColor(proposed_candidate.quad_iter,
                                                   SkColors::kTransparent,
                                                   SkBlendMode::kSrcOver);
  }
}

// Turn on blending for the output surface plane so the underlay could show
// through.
void OverlayStrategyUnderlay::AdjustOutputSurfaceOverlay(
    OverlayProcessorInterface::OutputSurfaceOverlayPlane*
        output_surface_plane) {
  if (output_surface_plane)
    output_surface_plane->enable_blending = true;
}

OverlayStrategy OverlayStrategyUnderlay::GetUMAEnum() const {
  return OverlayStrategy::kUnderlay;
}

}  // namespace viz
