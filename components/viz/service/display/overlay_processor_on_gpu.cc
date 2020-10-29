// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_on_gpu.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/ipc/display_compositor_memory_and_task_controller_on_gpu.h"
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
#if defined(OS_ANDROID)
  // TODO(weiliangc): Currently only implemented for Android Classic code path.
  for (auto& overlay : overlay_candidates) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceOverlay(overlay.mailbox);
    // When the display is re-opened, the first few frames might not have a
    // video resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;
    // In the current implementation, the BeginReadAccess will end up calling
    // CodecImage::RenderToOverlay. Currently this code path is only used for
    // Android Classic video overlay, where update of the overlay plane is
    // within the media code. Since we are not actually passing an overlay plane
    // to the display controller here, we are able to call EndReadAccess
    // directly after BeginReadAccess.
    shared_image_overlay->NotifyOverlayPromotion(
        true, ToNearestRect(overlay.display_rect));
    std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
        scoped_access = shared_image_overlay->BeginScopedReadAccess(
            false /* needs_gl_image */);
  }
#endif
}

#if defined(OS_ANDROID)
void OverlayProcessorOnGpu::NotifyOverlayPromotions(
    base::flat_set<gpu::Mailbox> promotion_denied,
    base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions) {
  for (auto& denied : promotion_denied) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceOverlay(denied);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(false, gfx::Rect());
  }
  for (auto& possible : possible_promotions) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceOverlay(possible.first);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(true, possible.second);
  }
}
#endif
}  // namespace viz
