// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT OverlayStrategySingleOnTop
    : public OverlayProcessorStrategy {
 public:
  explicit OverlayStrategySingleOnTop(
      OverlayProcessorUsingStrategy* capability_checker);

  OverlayStrategySingleOnTop(const OverlayStrategySingleOnTop&) = delete;
  OverlayStrategySingleOnTop& operator=(const OverlayStrategySingleOnTop&) =
      delete;

  ~OverlayStrategySingleOnTop() override;

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

  OverlayStrategy GetUMAEnum() const override;

 private:
  bool TryOverlay(AggregatedRenderPass* render_pass,
                  const PrimaryPlane* primary_plane,
                  OverlayCandidateList* candidate_list,
                  const OverlayProposedCandidate& proposed_candidate);

  raw_ptr<OverlayProcessorUsingStrategy> capability_checker_;  // Weak.

  ResourceId previous_frame_resource_id_ = kInvalidResourceId;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
