// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_

#include <memory>
#include <vector>

#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/viz_service_export.h"

#include "chromecast/media/service/mojom/video_geometry_setter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace viz {
// Similar to underlay strategy plus Cast-specific handling of content bounds.
class VIZ_SERVICE_EXPORT OverlayStrategyUnderlayCast
    : public OverlayStrategyUnderlay {
 public:
  explicit OverlayStrategyUnderlayCast(
      OverlayProcessorUsingStrategy* capability_checker);

  OverlayStrategyUnderlayCast(const OverlayStrategyUnderlayCast&) = delete;
  OverlayStrategyUnderlayCast& operator=(const OverlayStrategyUnderlayCast&) =
      delete;

  ~OverlayStrategyUnderlayCast() override;

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

  // In Chromecast build, OverlayStrategyUnderlayCast needs a valid mojo
  // interface to VideoGeometrySetter Service (shared by all instances of
  // OverlaystrategyUnderlayCast). This must be called before compositor starts.
  // Ideally, it can be called after compositor thread is created.
  // Must be called on compositor thread.
  static void ConnectVideoGeometrySetter(
      mojo::PendingRemote<chromecast::media::mojom::VideoGeometrySetter>
          video_geometry_setter);

  OverlayStrategy GetUMAEnum() const override;

 private:
  // Keep track if an overlay is being used on the previous frame.
  bool is_using_overlay_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_
