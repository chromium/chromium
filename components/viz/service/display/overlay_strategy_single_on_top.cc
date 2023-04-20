// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_single_on_top.h"

#include <vector>

#include "base/check_op.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "components/viz/service/display/overlay_proposed_candidate.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {

using OverlayProposedCandidateIndex =
    std::vector<OverlayProposedCandidate>::size_type;

// Calculates and caches for the candidates in the list between
// `single_on_top_candidates_start` (inclusively) and
// `single_on_top_candidates_end` (exclusively) if they are occluded by
// `candidate_with_display_masks`.
//
// Returns true if `candidate_with_display_masks` does occlude any other
// candidate.
bool CalculateOcclusionByRoundedDisplayMaskCandidate(
    OverlayProposedCandidate& candidate_with_display_masks,
    std::vector<OverlayProposedCandidate>* candidates,
    OverlayProposedCandidateIndex single_on_top_candidates_begin,
    OverlayProposedCandidateIndex single_on_top_candidates_end) {
  DCHECK(candidate_with_display_masks.candidate.has_rounded_display_masks);

  auto mask_bounds = OverlayProposedCandidate::GetRoundedDisplayMasksBounds(
      candidate_with_display_masks);

  bool intersects_candidate = false;
  for (OverlayProposedCandidateIndex i = single_on_top_candidates_begin;
       i < single_on_top_candidates_end; i++) {
    auto& overlap_candidate = candidates->at(i);

    // The rects are rounded as they're snapped by the compositor to pixel
    // unless it is AA'ed, in which case, it won't be overlaid.
    gfx::Rect overlap_rect =
        gfx::ToRoundedRect(OverlayCandidate::DisplayRectInTargetSpace(
            overlap_candidate.candidate));

    // Check that no candidate overlaps with any of painted masks. Quads
    // that have rounded-display masks, are all transparent except for the drawn
    // masks.
    for (const gfx::Rect& mask_bound : mask_bounds) {
      if (mask_bound.Intersects(overlap_rect)) {
        intersects_candidate = true;

        // Cache the occluding `candidate_with_display_masks` for the
        // overlap_candidate.
        overlap_candidate.occluding_mask_keys.insert(
            OverlayProposedCandidate::ToProposeKey(
                candidate_with_display_masks));
      }
    }
  }

  return intersects_candidate;
}

}  // namespace

OverlayStrategySingleOnTop::OverlayStrategySingleOnTop(
    OverlayProcessorUsingStrategy* capability_checker)
    : capability_checker_(capability_checker) {
  DCHECK(capability_checker);
}

OverlayStrategySingleOnTop::~OverlayStrategySingleOnTop() = default;

void OverlayStrategySingleOnTop::Propose(
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
  // Build a list of candidates with the associated quad.
  OverlayCandidateFactory candidate_factory = OverlayCandidateFactory(
      render_pass, resource_provider, surface_damage_rect_list,
      &output_color_matrix, GetPrimaryPlaneDisplayRect(primary_plane),
      &render_pass_filters,
      /*is_delegated_context=*/false, /*supports_clip_rect=*/false,
      /*supports_arbitrary_transform=*/false,
      /*supports_rounded_display_masks=*/true);

  std::vector<OverlayProposedCandidate> candidates_with_masks;

  QuadList::Iterator quads_with_masks_iter = quad_list->begin();
  while (quads_with_masks_iter != quad_list->end()) {
    OverlayCandidate candidate;

    // Quads with rounded-display masks are only considered as SingleOnTop
    // candidates if they are the top most candidates.
    if (candidate_factory.FromDrawQuad(*quads_with_masks_iter, candidate) ==
            OverlayCandidate::CandidateStatus::kSuccess &&
        !candidate.has_mask_filter && candidate.has_rounded_display_masks) {
      // Candidates with rounded-display masks should not overlap any other quad
      // with rounded-display masks.
      DCHECK(!candidate_factory.IsOccluded(candidate, quad_list->begin(),
                                           quads_with_masks_iter));
      candidates_with_masks.emplace_back(quads_with_masks_iter, candidate,
                                         this);
      quads_with_masks_iter++;
    } else {
      break;
    }
  }

  OverlayProposedCandidateIndex single_on_top_candidates_begin =
      candidates->size();

  QuadList::Iterator& quads_without_masks_begin = quads_with_masks_iter;
  for (auto it = quads_without_masks_begin; it != quad_list->end(); ++it) {
    OverlayCandidate candidate;

    // We exclude quads with rounded-display masks from the occlusion
    // calculation because they are drawn on top and will be promoted to
    // overlays if they occlude any SingleOnTop candidates.
    if (candidate_factory.FromDrawQuad(*it, candidate) ==
            OverlayCandidate::CandidateStatus::kSuccess &&
        !candidate.has_mask_filter &&
        !candidate_factory.IsOccluded(candidate, quads_without_masks_begin,
                                      it)) {
      DCHECK(!candidate.has_rounded_display_masks);
      candidates->emplace_back(it, candidate, this);
    }
  }

  DCHECK_LE(candidates_with_masks.size(), 2u);

  // To save power we can skip promoting candidates with rounded display mask
  // rects if they do not occlude any other SingleOnTop candidate.
  for (auto& mask_candidate : candidates_with_masks) {
    // We mark all the SingleOnTop candidates that are occluded by any mask
    // rounded-display masks to be later used in overlay processing.
    if (CalculateOcclusionByRoundedDisplayMaskCandidate(
            mask_candidate, candidates, single_on_top_candidates_begin,
            candidates->size())) {
      candidates->push_back(mask_candidate);
    }
  }
}

bool OverlayStrategySingleOnTop::Attempt(
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
  // Before we attempt an overlay strategy, we shouldn't have a candidate.
  DCHECK(candidate_list->empty());
  auto* render_pass = render_pass_list->back().get();
  return TryOverlay(render_pass, primary_plane, candidate_list,
                    proposed_candidate);
}

bool OverlayStrategySingleOnTop::TryOverlay(
    AggregatedRenderPass* render_pass,
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidate_list,
    const OverlayProposedCandidate& proposed_candidate) {
  // SingleOnTop strategy means we should have one candidate.
  DCHECK(candidate_list->empty());
  // Add the overlay.
  OverlayCandidateList new_candidate_list = *candidate_list;
  new_candidate_list.push_back(proposed_candidate.candidate);
  new_candidate_list.back().plane_z_order = 1;

  // Check for support.
  capability_checker_->CheckOverlaySupport(primary_plane, &new_candidate_list);

  const OverlayCandidate& overlay_candidate = new_candidate_list.back();
  // If the candidate can be handled by an overlay, create a pass for it.
  if (overlay_candidate.overlay_handled) {
    CommitCandidate(proposed_candidate, render_pass);
    candidate_list->swap(new_candidate_list);
    return true;
  }

  return false;
}

void OverlayStrategySingleOnTop::CommitCandidate(
    const OverlayProposedCandidate& proposed_candidate,
    AggregatedRenderPass* render_pass) {
  render_pass->quad_list.EraseAndInvalidateAllPointers(
      proposed_candidate.quad_iter);
}

OverlayStrategy OverlayStrategySingleOnTop::GetUMAEnum() const {
  return OverlayStrategy::kSingleOnTop;
}

}  // namespace viz
