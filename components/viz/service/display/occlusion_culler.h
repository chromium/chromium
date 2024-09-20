// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OCCLUSION_CULLER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OCCLUSION_CULLER_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class AggregatedFrame;
class OverlayProcessorInterface;

class VIZ_SERVICE_EXPORT OcclusionCuller {
 public:
  OcclusionCuller(OverlayProcessorInterface* overlay_processor,
                  const RendererSettings::OcclusionCullerSettings& settings);

  OcclusionCuller(const OcclusionCuller&) = delete;
  OcclusionCuller& operator=(const OcclusionCuller&) = delete;

  ~OcclusionCuller();

  void RemoveOverdrawQuads(AggregatedFrame* frame, float device_scale_factor);

 private:
  const raw_ptr<OverlayProcessorInterface> overlay_processor_;
  const RendererSettings::OcclusionCullerSettings settings_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OCCLUSION_CULLER_H_
