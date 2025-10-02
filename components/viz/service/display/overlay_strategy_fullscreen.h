// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_FULLSCREEN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_FULLSCREEN_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
// Overlay strategy to promote a single full screen quad to an overlay.
// The promoted quad should have all the property of the framebuffer and it
// should be possible to use it as such.
class VIZ_SERVICE_EXPORT OverlayStrategyFullscreen
    : public OverlayProcessorStrategy {
 public:
  explicit OverlayStrategyFullscreen(
      OverlayProcessorUsingStrategy* capability_checker);

  OverlayStrategyFullscreen(const OverlayStrategyFullscreen&) = delete;
  OverlayStrategyFullscreen& operator=(const OverlayStrategyFullscreen&) =
      delete;

  ~OverlayStrategyFullscreen() override;

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

  bool RemoveOutputSurfaceAsOverlay() override;
  OverlayStrategy GetUMAEnum() const override;

 private:
  raw_ptr<OverlayProcessorUsingStrategy> capability_checker_;  // Weak.
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_FULLSCREEN_H_
