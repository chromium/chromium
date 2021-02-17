// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
class VIZ_SERVICE_EXPORT OverlayProcessorWin
    : public OverlayProcessorInterface {
 public:
  using CandidateList = DCLayerOverlayList;

  OverlayProcessorWin(
      OutputSurface* output_surface,
      std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor);
  ~OverlayProcessorWin() override;

  bool IsOverlaySupported() const override;
  gfx::Rect GetPreviousFrameOverlaysBoundingRect() const override;
  gfx::Rect GetAndResetOverlayDamage() override;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  bool NeedsSurfaceDamageRectList() const override;

  void AdjustOutputSurfaceOverlay(base::Optional<OutputSurfaceOverlayPlane>*
                                      output_surface_plane) override {}

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) override;

  void set_using_dc_layers_for_testing(bool value) { using_dc_layers_ = value; }

 protected:
  // For testing.
  DCLayerOverlayProcessor* GetOverlayProcessor() {
    return dc_layer_overlay_processor_.get();
  }

 private:
  OutputSurface* const output_surface_;
  // Whether direct composition layers are being used with SetEnableDCLayers().
  bool using_dc_layers_ = false;
  // Number of frames since the last time direct composition layers were used.
  int frames_since_using_dc_layers_ = 0;

  // TODO(weiliangc): Eventually fold DCLayerOverlayProcessor into this class.
  std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor_;

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessorWin);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_
