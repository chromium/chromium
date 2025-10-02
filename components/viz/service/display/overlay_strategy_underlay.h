// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// The underlay strategy looks for a video quad without regard to quads above
// it. The video is "underlaid" through a black transparent quad substituted
// for the video quad. The overlay content can then be blended in by the
// hardware under the the scene. This is only valid for overlay contents that
// are fully opaque.
class VIZ_SERVICE_EXPORT OverlayStrategyUnderlay
    : public OverlayProcessorStrategy {
 public:
  enum class OpaqueMode {
    // Require candidates to be |is_opaque|.
    RequireOpaqueCandidates,

    // Allow non-|is_opaque| candidates to be promoted.
    AllowTransparentCandidates,
  };

  // If |allow_nonopaque_overlays| is true, then we don't require that the
  // the candidate is_opaque.
  explicit OverlayStrategyUnderlay(
      OverlayProcessorUsingStrategy* capability_checker,
      OpaqueMode opaque_mode = OpaqueMode::RequireOpaqueCandidates);

  OverlayStrategyUnderlay(const OverlayStrategyUnderlay&) = delete;
  OverlayStrategyUnderlay& operator=(const OverlayStrategyUnderlay&) = delete;

  ~OverlayStrategyUnderlay() override;

  void Propose(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      std::vector<OverlayProposedCandidate>* candidates,
      std::vector<gfx::Rect>* content_bounds) override;

  bool Attempt(
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
      const OverlayProposedCandidate& proposed_candidate) override;

  void CommitCandidate(const OverlayProposedCandidate& proposed_candidate,
                       AggregatedRenderPass* render_pass) override;

  void AdjustOutputSurfaceOverlay(
      OverlayProcessorInterface::OutputSurfaceOverlayPlane*
          output_surface_plane) override;

  OverlayStrategy GetUMAEnum() const override;

 private:
  raw_ptr<OverlayProcessorUsingStrategy> capability_checker_;  // Weak.
  OpaqueMode opaque_mode_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_H_
