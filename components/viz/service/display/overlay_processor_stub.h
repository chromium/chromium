// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STUB_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STUB_H_

#include <vector>

#include "components/viz/service/display/overlay_processor_interface.h"

namespace viz {
// This is a stub class that implements OverlayProcessorInterface that is used
// for platforms that don't support overlays.
class VIZ_SERVICE_EXPORT OverlayProcessorStub
    : public OverlayProcessorInterface {
 public:
  OverlayProcessorStub() : OverlayProcessorInterface() {}

  OverlayProcessorStub(const OverlayProcessorStub&) = delete;
  OverlayProcessorStub& operator=(const OverlayProcessorStub&) = delete;

  ~OverlayProcessorStub() override {}

  // Overrides OverlayProcessorInterface's pure virtual functions.
  bool IsOverlaySupported() const final;
  gfx::Rect GetPreviousFrameOverlaysBoundingRect() const final;
  gfx::Rect GetAndResetOverlayDamage() final;
  bool NeedsSurfaceDamageRectList() const final;
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
      std::vector<gfx::Rect>* content_bounds) final {}
  void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) final {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) final {}
  void SetViewportSize(const gfx::Size& size) final {}
  gfx::CALayerResult GetCALayerErrorCode() const final;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STUB_H_
