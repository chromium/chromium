// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_vulkan_secondary_cb.h"

#include <utility>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/private/chromium/GrVkSecondaryCBDrawContext.h"
#include "ui/gfx/presentation_feedback.h"

namespace viz {

SkiaOutputDeviceVulkanSecondaryCB::SkiaOutputDeviceVulkanSecondaryCB(
    VulkanContextProvider* context_provider,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_provider->GetGrContext(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      context_provider_(context_provider) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.pending_swap_params.max_pending_swaps = 1;
  capabilities_.preserve_buffer_content = false;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities_.supports_post_sub_buffer = false;
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kLogic;
  capabilities_.root_is_vulkan_secondary_command_buffer = true;

  GrVkSecondaryCBDrawContext* secondary_cb_draw_context =
      context_provider_->GetGrSecondaryCBDrawContext();
  SkSurfaceCharacterization characterization;
  VkFormat vkFormat = VK_FORMAT_UNDEFINED;
  bool result = secondary_cb_draw_context->characterize(&characterization);
  CHECK(result);
  characterization.backendFormat().asVkFormat(&vkFormat);
  auto sk_color_type = vkFormat == VK_FORMAT_R8G8B8A8_UNORM
                           ? kRGBA_8888_SkColorType
                           : kBGRA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      sk_color_type;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      sk_color_type;
}

std::unique_ptr<SkiaOutputDevice::ScopedPaint>
SkiaOutputDeviceVulkanSecondaryCB::BeginScopedPaint() {
  std::vector<GrBackendSemaphore> end_semaphores;
  SkSurface* sk_surface = BeginPaint(&end_semaphores);
  return std::make_unique<SkiaOutputDevice::ScopedPaint>(
      std::move(end_semaphores), this, sk_surface);
}

void SkiaOutputDeviceVulkanSecondaryCB::Submit(bool sync_cpu,
                                               base::OnceClosure callback) {
  // Submit the primary command buffer which may render passes.
  context_provider_->GetGrContext()->submit(sync_cpu);
  context_provider_->EnqueueSecondaryCBPostSubmitTask(std::move(callback));
}

bool SkiaOutputDeviceVulkanSecondaryCB::Reshape(
    const SkImageInfo& image_info,
    const gfx::ColorSpace& color_space,
    int sample_count,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  // No-op
  size_ = gfx::SkISizeToSize(image_info.dimensions());
  return true;
}

void SkiaOutputDeviceVulkanSecondaryCB::Present(
    const absl::optional<gfx::Rect>& update_rect,
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
  CHECK(!update_rect);
  StartSwapBuffers(std::move(feedback));
  FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK), size_,
                    std::move(frame));
}

SkSurface* SkiaOutputDeviceVulkanSecondaryCB::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  return nullptr;
}

void SkiaOutputDeviceVulkanSecondaryCB::EndPaint() {}

SkCanvas* SkiaOutputDeviceVulkanSecondaryCB::GetCanvas(SkSurface* sk_surface) {
  DCHECK(!sk_surface);
  return context_provider_->GetGrSecondaryCBDrawContext()->getCanvas();
}

GrSemaphoresSubmitted SkiaOutputDeviceVulkanSecondaryCB::Flush(
    SkSurface* sk_surface,
    VulkanContextProvider* vulkan_context_provider,
    std::vector<GrBackendSemaphore> end_semaphores,
    base::OnceClosure on_finished) {
  DCHECK(!sk_surface);
  DCHECK_EQ(context_provider_, vulkan_context_provider);

  std::vector<VkSemaphore> vk_end_semaphores;
  for (const GrBackendSemaphore& gr_semaphore : end_semaphores) {
    vk_end_semaphores.push_back(gr_semaphore.vkSemaphore());
  }
  vulkan_context_provider->EnqueueSecondaryCBSemaphores(
      std::move(vk_end_semaphores));
  if (on_finished) {
    vulkan_context_provider->EnqueueSecondaryCBPostSubmitTask(
        std::move(on_finished));
  }
  vulkan_context_provider->GetGrSecondaryCBDrawContext()->flush();
  return GrSemaphoresSubmitted::kYes;
}

bool SkiaOutputDeviceVulkanSecondaryCB::Wait(
    SkSurface* sk_surface,
    int num_semaphores,
    const GrBackendSemaphore wait_semaphores[],
    bool delete_semaphores_after_wait) {
  DCHECK(!sk_surface);
  return context_provider_->GetGrSecondaryCBDrawContext()->wait(
      num_semaphores, wait_semaphores, delete_semaphores_after_wait);
}

bool SkiaOutputDeviceVulkanSecondaryCB::Draw(
    SkSurface* sk_surface,
    sk_sp<const SkDeferredDisplayList> ddl) {
  DCHECK(!sk_surface);
  return context_provider_->GetGrSecondaryCBDrawContext()->draw(ddl);
}

}  // namespace viz
