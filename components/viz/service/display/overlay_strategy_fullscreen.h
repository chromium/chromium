// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_FULLSCREEN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_FULLSCREEN_H_

#include "base/macros.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class OverlayCandidateValidator;
// Overlay strategy to promote a single full screen quad to an overlay.
// The promoted quad should have all the property of the framebuffer and it
// should be possible to use it as such.
class VIZ_SERVICE_EXPORT OverlayStrategyFullscreen
    : public OverlayProcessor::Strategy {
 public:
  explicit OverlayStrategyFullscreen(
      OverlayCandidateValidator* capability_checker);
  ~OverlayStrategyFullscreen() override;

  bool Attempt(
      const SkMatrix44& output_color_matrix,
      const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_pass,
      const PrimaryPlane* primary_plane,
      OverlayCandidateList* candidate_list,
      std::vector<gfx::Rect>* content_bounds) override;

  bool RemoveOutputSurfaceAsOverlay() override;
  OverlayStrategy GetUMAEnum() const override;

 private:
  OverlayCandidateValidator* capability_checker_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyFullscreen);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_FULLSCREEN_H_
