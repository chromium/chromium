// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STRATEGY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STRATEGY_H_

#include <memory>
#include <vector>

#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_proposed_candidate.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// A strategy for promoting a DrawQuad(s) to overlays. Used by
// `OverlayProcessorUsingStrategy` for proposing and attempting candidates quads
// in different ways.
class VIZ_SERVICE_EXPORT OverlayProcessorStrategy {
 public:
  virtual ~OverlayProcessorStrategy() = default;
  using PrimaryPlane = OverlayProcessorInterface::OutputSurfaceOverlayPlane;

  // Appends all legitimate overlay candidates to the list |candidates|
  // for this strategy.  It is very important to note that this function
  // should not attempt a specific candidate it should merely identify them
  // and save the necessary data required to for a later attempt.
  virtual void Propose(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      std::vector<OverlayProposedCandidate>* candidates,
      std::vector<gfx::Rect>* content_bounds) = 0;

  // Returns false if the specific |proposed_candidate| cannot be made to work
  // for this strategy with the current set of render passes. Returns true if
  // the strategy was successful and adds any additional passes necessary to
  // represent overlays to |render_pass_list|. Most strategies should look at
  // the primary RenderPass, the last element.
  virtual bool Attempt(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds,
      const OverlayProposedCandidate& proposed_candidate) = 0;

  // Commits to using the proposed candidate by updating |render_pass| as
  // appropriate when this candidate is presented in an overlay plane.
  virtual void CommitCandidate(
      const OverlayProposedCandidate& proposed_candidate,
      AggregatedRenderPass* render_pass) = 0;

  // Currently this is only overridden by the Underlay strategy: the underlay
  // strategy needs to enable blending for the primary plane in order to show
  // content underneath.
  virtual void AdjustOutputSurfaceOverlay(
      OverlayProcessorInterface::OutputSurfaceOverlayPlane*
          output_surface_plane) {}

  // Currently this is only overridden by the Fullscreen strategy: the
  // fullscreen strategy covers the entire screen and there is no need to use
  // the primary plane.
  virtual bool RemoveOutputSurfaceAsOverlay();

  virtual OverlayStrategy GetUMAEnum() const;

  // Does a null-check on |primary_plane| and returns it's |display_rect|
  // member if non-null and an empty gfx::RectF otherwise.
  gfx::RectF GetPrimaryPlaneDisplayRect(const PrimaryPlane* primary_plane);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STRATEGY_H_
