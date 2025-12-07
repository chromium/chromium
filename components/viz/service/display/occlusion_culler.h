// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OCCLUSION_CULLER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OCCLUSION_CULLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
class DisplayResourceProvider;
class DrawQuad;
class AggregatedFrame;
class OverlayProcessorInterface;

class VIZ_SERVICE_EXPORT OcclusionCuller {
 public:
  OcclusionCuller(OverlayProcessorInterface* overlay_processor,
                  DisplayResourceProvider* resource_provider,
                  const RendererSettings::OcclusionCullerSettings& settings);

  OcclusionCuller(const OcclusionCuller&) = delete;
  OcclusionCuller& operator=(const OcclusionCuller&) = delete;

  ~OcclusionCuller();

  void UpdateDeviceScaleFactor(float device_scale_factor);
  void RemoveOverdrawQuads(AggregatedFrame* frame);

 private:
  // Decides whether or not a DrawQuad should be split into a more complex
  // visible region in order to avoid overdraw.
  bool CanSplitDrawQuad(const DrawQuad* quad,
                        const gfx::Size& visible_region_bounding_size,
                        const std::vector<gfx::Rect>& visible_region_rects);

  float device_scale_factor_ = 1.0f;

  const raw_ptr<OverlayProcessorInterface> overlay_processor_;
  const raw_ptr<DisplayResourceProvider> resource_provider_;
  const RendererSettings::OcclusionCullerSettings settings_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OCCLUSION_CULLER_H_
