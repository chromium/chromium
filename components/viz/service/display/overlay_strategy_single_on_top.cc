// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_single_on_top.h"

#include <vector>

#include "components/viz/service/display/overlay_candidate_factory.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayStrategySingleOnTop::OverlayStrategySingleOnTop(
    OverlayProcessorUsingStrategy* capability_checker)
    : capability_checker_(capability_checker) {
  DCHECK(capability_checker);
}

OverlayStrategySingleOnTop::~OverlayStrategySingleOnTop() {}

bool OverlayStrategySingleOnTop::Attempt(
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  // Before we attempt an overlay strategy, we shouldn't have a candidate.
  DCHECK(candidate_list->empty());
  auto* render_pass = render_pass_list->back().get();
  QuadList* quad_list = &render_pass->quad_list;
  // Build a list of candidates with the associated quad.
  OverlayCandidate best_candidate;
  OverlayCandidateFactory candidate_factory = OverlayCandidateFactory(
      render_pass, resource_provider, surface_damage_rect_list,
      &output_color_matrix, GetPrimaryPlaneDisplayRect(primary_plane));

  auto best_quad_it = quad_list->end();
  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    OverlayCandidate candidate;
    if (candidate_factory.FromDrawQuad(*it, candidate) ==
            OverlayCandidate::CandidateStatus::kSuccess &&
        !candidate.has_mask_filter &&
        !OverlayCandidate::IsOccluded(candidate, quad_list->cbegin(), it)) {
      // If the candidate has been promoted previously and has not changed
      // (resource ID is the same) for 3 frames, do not use it as Overlay as
      // flattening it to the main fb will be more power efficient when the
      // contents don't change.
      if (candidate.resource_id == previous_frame_resource_id_ &&
          ++same_resource_id_frames_count_ >
              kMaxFrameCandidateWithSameResourceId) {
        continue;
      }
      if (candidate.display_rect.size().GetArea() >
          best_candidate.display_rect.size().GetArea()) {
        best_candidate = candidate;
        best_quad_it = it;
      }
    }
  }
  if (best_quad_it == quad_list->end())
    return false;

  OverlayProposedCandidate proposed_candidate(best_quad_it, best_candidate,
                                              this);
  if (TryOverlay(render_pass, primary_plane, candidate_list,
                 proposed_candidate)) {
    if (previous_frame_resource_id_ != best_candidate.resource_id) {
      previous_frame_resource_id_ = best_candidate.resource_id;
      same_resource_id_frames_count_ = 1;
    }
    return true;
  } else {
    previous_frame_resource_id_ = kInvalidResourceId;
    same_resource_id_frames_count_ = 0;
    return false;
  }
}

void OverlayStrategySingleOnTop::ProposePrioritized(
    const SkM44& output_color_matrix,
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
      &output_color_matrix, GetPrimaryPlaneDisplayRect(primary_plane));

  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    OverlayCandidate candidate;
    if (candidate_factory.FromDrawQuad(*it, candidate) ==
            OverlayCandidate::CandidateStatus::kSuccess &&
        !candidate.has_mask_filter &&
        !OverlayCandidate::IsOccluded(candidate, quad_list->cbegin(), it)) {
      candidates->push_back({it, candidate, this});
    }
  }
}

bool OverlayStrategySingleOnTop::AttemptPrioritized(
    const SkM44& output_color_matrix,
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
