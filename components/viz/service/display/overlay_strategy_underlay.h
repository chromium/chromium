// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_H_

#include <vector>

#include "base/macros.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// The underlay strategy looks for a video quad without regard to quads above
// it. The video is "underlaid" through a black transparent quad substituted
// for the video quad. The overlay content can then be blended in by the
// hardware under the the scene. This is only valid for overlay contents that
// are fully opaque.
class VIZ_SERVICE_EXPORT OverlayStrategyUnderlay
    : public OverlayProcessorUsingStrategy::Strategy {
 public:
  enum class OpaqueMode {
    // Require candidates to be |is_opaque|.
    RequireOpaqueCandidates,

    // Allow non-|is_opaque| candidates to be promoted.
    AllowTransparentCandidates,
  };

  // If |allow_nonopaque_overlays| is true, then we don't require that the
  // the candidate is_opaque.
  OverlayStrategyUnderlay(
      OverlayProcessorUsingStrategy* capability_checker,
      OpaqueMode opaque_mode = OpaqueMode::RequireOpaqueCandidates);
  ~OverlayStrategyUnderlay() override;

  bool Attempt(const SkMatrix44& output_color_matrix,
               const OverlayProcessorInterface::FilterOperationsMap&
                   render_pass_backdrop_filters,
               DisplayResourceProvider* resource_provider,
               AggregatedRenderPassList* render_pass,
               SurfaceDamageRectList* surface_damage_rect_list,
               const PrimaryPlane* primary_plane,
               OverlayCandidateList* candidate_list,
               std::vector<gfx::Rect>* content_bounds) override;

  void ProposePrioritized(const SkMatrix44& output_color_matrix,
                          const OverlayProcessorInterface::FilterOperationsMap&
                              render_pass_backdrop_filters,
                          DisplayResourceProvider* resource_provider,
                          AggregatedRenderPassList* render_pass_list,
                          SurfaceDamageRectList* surface_damage_rect_list,
                          const PrimaryPlane* primary_plane,
                          OverlayProposedCandidateList* candidates,
                          std::vector<gfx::Rect>* content_bounds) override;

  bool AttemptPrioritized(
      const SkMatrix44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds,
      OverlayProposedCandidate* proposed_candidate) override;

  void AdjustOutputSurfaceOverlay(
      OverlayProcessorInterface::OutputSurfaceOverlayPlane*
          output_surface_plane) override;

  OverlayStrategy GetUMAEnum() const override;

 private:
  OverlayProcessorUsingStrategy* capability_checker_;  // Weak.
  OpaqueMode opaque_mode_;

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyUnderlay);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_H_
