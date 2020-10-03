// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
// OverlayProcessor subclass that goes through a list of strategies to determine
// overlay candidates. THis is used by Android and Ozone platforms.
class VIZ_SERVICE_EXPORT OverlayProcessorUsingStrategy
    : public OverlayProcessorInterface {
 public:
  using CandidateList = OverlayCandidateList;

  // TODO(weiliangc): Move it to an external class.
  class VIZ_SERVICE_EXPORT Strategy {
   public:
    virtual ~Strategy() {}
    using PrimaryPlane = OverlayProcessorInterface::OutputSurfaceOverlayPlane;
    // Returns false if the strategy cannot be made to work with the
    // current set of render passes. Returns true if the strategy was successful
    // and adds any additional passes necessary to represent overlays to
    // |render_pass_list|. Most strategies should look at the primary
    // RenderPass, the last element.
    virtual bool Attempt(
        const SkMatrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        AggregatedRenderPassList* render_pass_list,
        SurfaceDamageRectList* surface_damage_rect_list,
        const PrimaryPlane* primary_plane,
        OverlayCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) = 0;

    // Currently this is only overridden by the Underlay strategy: the underlay
    // strategy needs to enable blending for the primary plane in order to show
    // content underneath.
    virtual void AdjustOutputSurfaceOverlay(
        OutputSurfaceOverlayPlane* output_surface_plane) {}

    // Currently this is only overridden by the Fullscreen strategy: the
    // fullscreen strategy covers the entire screen and there is no need to use
    // the primary plane.
    virtual bool RemoveOutputSurfaceAsOverlay();

    virtual OverlayStrategy GetUMAEnum() const;
  };
  using StrategyList = std::vector<std::unique_ptr<Strategy>>;

  ~OverlayProcessorUsingStrategy() override;

  gfx::Rect GetPreviousFrameOverlaysBoundingRect() const final;
  gfx::Rect GetAndResetOverlayDamage() final;

  // Override OverlayProcessor.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetViewportSize(const gfx::Size& size) override {}

  // Attempt to replace quads from the specified root render pass with overlays.
  // This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList* surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) final;

  // This function takes a pointer to the base::Optional instance so the
  // instance can be reset. When overlay strategy covers the entire output
  // surface, we no longer need the output surface as a separate overlay. This
  // is also used by SurfaceControl to adjust rotation.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  void AdjustOutputSurfaceOverlay(
      base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;

  OverlayProcessorUsingStrategy();

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. Candidates with |overlay_handled| set to
  // true must also have their |display_rect| converted to integer
  // coordinates if necessary.
  virtual void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidate_list) = 0;

 protected:
  virtual gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const;

  StrategyList strategies_;
  Strategy* last_successful_strategy_ = nullptr;

  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_underlay_rect_;
  bool previous_frame_underlay_was_unoccluded_ = false;

 private:
  // Update |damage_rect| by removing damage caused by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        const gfx::Rect& previous_frame_underlay_rect,
                        bool previous_frame_underlay_was_unoccluded,
                        const QuadList* quad_list,
                        gfx::Rect* damage_rect);

  // Iterate through a list of strategies and attempt to overlay with each.
  // Returns true if one of the attempts is successful. Has to be called after
  // InitializeStrategies(). A |primary_plane| represents the output surface's
  // buffer that comes from |BufferQueue|. It is passed in here so it could be
  // pass through to hardware through CheckOverlaySupport. It is not passed in
  // as a const member because the underlay strategy changes the
  // |primary_plane|'s blending setting.
  bool AttemptWithStrategies(
      const SkMatrix44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds);

  // Used by Android pre-SurfaceControl to notify promotion hints.
  virtual void NotifyOverlayPromotion(
      DisplayResourceProvider* display_resource_provider,
      const OverlayCandidateList& candidate_list,
      const QuadList& quad_list);

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessorUsingStrategy);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_
