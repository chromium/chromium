// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_on_gpu.h"

#include "build/build_config.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayProcessorOnGpu::OverlayProcessorOnGpu(
    gpu::DisplayCompositorMemoryAndTaskControllerOnGpu*
        display_controller_on_gpu)
    : shared_image_representation_factory_(
          std::make_unique<gpu::SharedImageRepresentationFactory>(
              display_controller_on_gpu->shared_image_manager(),
              display_controller_on_gpu->memory_tracker())) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

OverlayProcessorOnGpu::~OverlayProcessorOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void OverlayProcessorOnGpu::ScheduleOverlays(
    CandidateList&& overlay_candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  // TODO(weiliangc): Currently only implemented for Android Classic code path.
  for (auto& overlay : overlay_candidates) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceLegacyOverlay(
            overlay.mailbox);
    // When the display is re-opened, the first few frames might not have a
    // video resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;
    // RenderToOverlay() will notify media code to update frame in
    // SurfaceView/Dialog.
    shared_image_overlay->NotifyOverlayPromotion(
        true, ToNearestRect(overlay.display_rect));
    shared_image_overlay->RenderToOverlay();
  }
#endif
}

#if BUILDFLAG(IS_ANDROID)
void OverlayProcessorOnGpu::NotifyOverlayPromotions(
    base::flat_set<gpu::Mailbox> promotion_denied,
    base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions) {
  for (auto& denied : promotion_denied) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceLegacyOverlay(denied);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(false, gfx::Rect());
  }
  for (auto& possible : possible_promotions) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceLegacyOverlay(
            possible.first);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(true, possible.second);
  }
}
#endif
}  // namespace viz
