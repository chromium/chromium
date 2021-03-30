// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_vulkan_secondary_cb_offscreen.h"

#include <utility>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/src/gpu/vk/GrVkSecondaryCBDrawContext.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace viz {

SkiaOutputDeviceVulkanSecondaryCBOffscreen::
    SkiaOutputDeviceVulkanSecondaryCBOffscreen(
        scoped_refptr<gpu::SharedContextState> context_state,
        gpu::MemoryTracker* memory_tracker,
        DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDeviceOffscreen(std::move(context_state),
                                gfx::SurfaceOrigin::kTopLeft,
                                /*has_alpha=*/true,
                                memory_tracker,
                                std::move(did_swap_buffer_complete_callback)) {
  DCHECK(context_state_->vk_context_provider());
  capabilities_.max_frames_pending = 1;
  capabilities_.preserve_buffer_content = false;
  capabilities_.supports_post_sub_buffer = false;
}

SkiaOutputDeviceVulkanSecondaryCBOffscreen::
    ~SkiaOutputDeviceVulkanSecondaryCBOffscreen() = default;

SkSurface* SkiaOutputDeviceVulkanSecondaryCBOffscreen::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  SkSurface* sk_surface = SkiaOutputDeviceOffscreen::BeginPaint(end_semaphores);
  sk_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  return sk_surface;
}

void SkiaOutputDeviceVulkanSecondaryCBOffscreen::SwapBuffers(
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
  StartSwapBuffers(std::move(feedback));

  auto format_index = static_cast<int>(format_);
  const auto& sk_color_type = capabilities_.sk_color_types[format_index];
  DCHECK(sk_color_type != kUnknown_SkColorType)
      << "SkColorType is invalid for format: " << format_index;
  sk_sp<SkImage> sk_image = SkImage::MakeFromTexture(
      context_state_->vk_context_provider()->GetGrContext(), backend_texture_,
      kTopLeft_GrSurfaceOrigin, sk_color_type, kPremul_SkAlphaType,
      sk_color_space_);
  gfx::SwapResult result = gfx::SwapResult::SWAP_ACK;
  if (sk_image) {
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrcOver);
    context_state_->vk_context_provider()
        ->GetGrSecondaryCBDrawContext()
        ->getCanvas()
        ->drawImage(sk_image, 0, 0, SkSamplingOptions(), &paint);
    context_state_->vk_context_provider()
        ->GetGrSecondaryCBDrawContext()
        ->flush();
  } else {
    result = gfx::SwapResult::SWAP_FAILED;
  }

  FinishSwapBuffers(gfx::SwapCompletionResult(result),
                    gfx::Size(size_.width(), size_.height()), std::move(frame));
}

}  // namespace viz
