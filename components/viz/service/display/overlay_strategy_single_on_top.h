// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_

#include <vector>

#include "base/macros.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT OverlayStrategySingleOnTop
    : public OverlayProcessorUsingStrategy::Strategy {
 public:
  explicit OverlayStrategySingleOnTop(
      OverlayProcessorUsingStrategy* capability_checker);
  ~OverlayStrategySingleOnTop() override;

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

  OverlayStrategy GetUMAEnum() const override;

 private:
  static constexpr size_t kMaxFrameCandidateWithSameResourceId = 3;

  bool TryOverlay(QuadList* quad_list,
                  const PrimaryPlane* primary_plane,
                  OverlayCandidateList* candidate_list,
                  const OverlayCandidate& candidate,
                  QuadList::Iterator candidate_iterator);

  OverlayProcessorUsingStrategy* capability_checker_;  // Weak.

  ResourceId previous_frame_resource_id_ = kInvalidResourceId;
  size_t same_resource_id_frames_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategySingleOnTop);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
