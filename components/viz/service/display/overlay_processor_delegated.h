// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_DELEGATED_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_DELEGATED_H_

#include <memory>
#include <vector>

#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_delegated_support.h"
#include "components/viz/service/display/overlay_processor_ozone.h"
#include "components/viz/service/viz_service_export.h"

#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace viz {

// OverlayProcessor subclass that attempts to promote to overlay all the draw
// quads of the root render pass. This is currently only used by LaCros.
// TODO(petermcneeley): This class and its Apple equivalent(s) will eventually
// be refactored in merged together into a unified delegation processor.
// Delegation will just become an extended feature of ozone and we avoid/push
// down platform specific defines and files where possible.
class VIZ_SERVICE_EXPORT OverlayProcessorDelegated
    : public OverlayProcessorOzone {
 public:
  OverlayProcessorDelegated(
      std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
      std::vector<OverlayStrategy> available_strategies,
      gpu::SharedImageInterface* shared_image_interface);
  OverlayProcessorDelegated(const OverlayProcessorDelegated&) = delete;
  OverlayProcessorDelegated& operator=(const OverlayProcessorDelegated&) =
      delete;
  ~OverlayProcessorDelegated() override;

  bool DisableSplittingQuads() const override;

  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) final;

  // This function takes a pointer to the std::optional instance so the
  // instance can be reset. When the overlay strategy covers the entire output
  // surface, we no longer need the output surface as a separate overlay. This
  // is also used by SurfaceControl to adjust rotation.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;

  gfx::RectF GetUnassignedDamage() const override;

 private:
  gfx::RectF GetPrimaryPlaneDisplayRect(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane*
          primary_plane);
  // Iterate through a list of strategies and attempt to overlay with each.
  // Returns true if one of the attempts is successful. Has to be called after
  // InitializeStrategies(). A |primary_plane| represents the output surface's
  // buffer that comes from |BufferQueue|. It is passed in here so it could be
  // pass through to hardware through CheckOverlaySupport. It is not passed
  // through as a const member because the underlay strategy changes the
  // |primary_plane|'s blending setting.
  bool AttemptWithStrategies(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds);

  // Should delegation be blocked because we have recently had copy output
  // requests on any render passes. The root render pass must not be delegated
  // if there is a copy request in order to draw correctly. For non-root passes,
  // this is done to prevent execessive power usage that can occur if copy
  // output requests happen approximately every other frame, causing a lot of
  // delegation overhead.
  bool BlockForCopyRequests(const AggregatedRenderPassList* render_pass_list);

  DelegationStatus delegated_status_ = DelegationStatus::kCompositedOther;
  bool supports_clip_rect_ = false;
  bool supports_out_of_window_clip_rect_ = false;
  bool needs_background_image_ = false;
  bool supports_affine_transform_ = false;
  bool has_transformation_fix_ = false;
  gfx::RectF unassigned_damage_;
  // Used to count the number of frames we should wait until allowing delegation
  // again.
  int copy_request_counter_ = 0;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_DELEGATED_H_
