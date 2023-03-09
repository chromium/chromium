// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ON_GPU_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

namespace gpu {
class DisplayCompositorMemoryAndTaskControllerOnGpu;
class SharedImageRepresentationFactory;
}  // namespace gpu

namespace viz {
// This class defines the gpu thread side functionalities of overlay processing.
// This class would receive a list of overlay candidates and schedule to present
// the overlay candidates every frame. This class is created, accessed, and
// destroyed on the gpu thread.
class VIZ_SERVICE_EXPORT OverlayProcessorOnGpu {
 public:
  using CandidateList = OverlayCandidateList;

  explicit OverlayProcessorOnGpu(
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu*
          display_controller_on_gpu);

  OverlayProcessorOnGpu(const OverlayProcessorOnGpu&) = delete;
  OverlayProcessorOnGpu& operator=(const OverlayProcessorOnGpu&) = delete;

  ~OverlayProcessorOnGpu();

  // This function takes the overlay candidates, and schedule them for
  // presentation later.
  void ScheduleOverlays(CandidateList&& overlay_candidates);

#if BUILDFLAG(IS_ANDROID)
  void NotifyOverlayPromotions(
      base::flat_set<gpu::Mailbox> promotion_denied,
      base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions);
#endif

 private:
  // TODO(weiliangc): Figure out how to share MemoryTracker with OutputSurface.
  // For now this class is only used for Android classic code path, which only
  // reads the shared images created elsewhere.
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ON_GPU_H_
