// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_MAC_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_MAC_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
class VIZ_SERVICE_EXPORT OverlayProcessorMac
    : public OverlayProcessorInterface {
 public:
  using CandidateList = OverlayCandidateList;

  OverlayProcessorMac();
  // For testing.
  explicit OverlayProcessorMac(
      std::unique_ptr<CALayerOverlayProcessor> ca_layer_overlay_processor);

  OverlayProcessorMac(const OverlayProcessorMac&) = delete;
  OverlayProcessorMac& operator=(const OverlayProcessorMac&) = delete;

  ~OverlayProcessorMac() override;

  bool DisableSplittingQuads() const override;

  bool IsOverlaySupported() const override;
  gfx::Rect GetPreviousFrameOverlaysBoundingRect() const override;
  gfx::Rect GetAndResetOverlayDamage() override;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  bool NeedsSurfaceDamageRectList() const override;

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
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
      std::vector<gfx::Rect>* content_bounds) override;

  // For Mac, if we successfully generated a candidate list for CALayerOverlay,
  // we no longer need the |output_surface_plane|. This function takes a pointer
  // to the std::optional instance so the instance can be reset.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;

  gfx::CALayerResult GetCALayerErrorCode() const override;

 private:
  // The damage that should be added the next frame for drawing to the output
  // surface.
  gfx::Rect ca_overlay_damage_rect_;
  gfx::Rect previous_frame_full_bounding_rect_;

 protected:
  // Protected for testing.
  // TODO(weiliangc): Eventually fold the CaLayerOverlayProcessor into this
  // class.
  std::unique_ptr<CALayerOverlayProcessor> ca_layer_overlay_processor_;
  CALayerOverlayProcessor* GetOverlayProcessor() const {
    return ca_layer_overlay_processor_.get();
  }

 private:
  bool output_surface_already_handled_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_MAC_H_
