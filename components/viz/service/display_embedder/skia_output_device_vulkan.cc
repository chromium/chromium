// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"

#include <utility>

#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"

namespace viz {

SkiaOutputDeviceVulkan::SkiaOutputDeviceVulkan(
    VulkanContextProvider* context_provider,
    gpu::SurfaceHandle surface_handle,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(true /*need_swap_semaphore */,
                       did_swap_buffer_complete_callback),
      context_provider_(context_provider),
      surface_handle_(surface_handle) {
  capabilities_.flipped_output_surface = true;
  capabilities_.supports_post_sub_buffer = false;
  capabilities_.supports_pre_transform = true;
}

SkiaOutputDeviceVulkan::~SkiaOutputDeviceVulkan() {
  DCHECK(!scoped_write_);
  if (vulkan_surface_) {
    auto* fence_helper = context_provider_->GetDeviceQueue()->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(vulkan_surface_));
  }
}

bool SkiaOutputDeviceVulkan::Reshape(const gfx::Size& size,
                                     float device_scale_factor,
                                     const gfx::ColorSpace& color_space,
                                     bool has_alpha,
                                     gfx::OverlayTransform transform) {
  DCHECK(!scoped_write_);

  uint32_t generation = 0;
  if (!vulkan_surface_) {
    CreateVulkanSurface();
  } else {
    generation = vulkan_surface_->swap_chain_generation();
  }

  vulkan_surface_->Reshape(size, transform);

  auto sk_color_space = color_space.ToSkColorSpace();
  if (vulkan_surface_->swap_chain_generation() != generation ||
      !SkColorSpace::Equals(sk_color_space.get(), sk_color_space_.get())) {
    // swapchain is changed, we need recreate all cached sk surfaces.
    sk_surfaces_.clear();
    sk_surfaces_.resize(vulkan_surface_->swap_chain()->num_images());
    sk_color_space_ = std::move(sk_color_space);
  }
  return true;
}

void SkiaOutputDeviceVulkan::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  // Reshape should have been called first.
  DCHECK(vulkan_surface_);
  DCHECK(!scoped_write_);

  StartSwapBuffers(std::move(feedback));
  auto image_size = vulkan_surface_->image_size();
  FinishSwapBuffers(vulkan_surface_->SwapBuffers(), image_size,
                    std::move(latency_info));
}

SkSurface* SkiaOutputDeviceVulkan::BeginPaint() {
  DCHECK(vulkan_surface_);
  DCHECK(!scoped_write_);

  scoped_write_.emplace(vulkan_surface_->swap_chain());
  if (!scoped_write_->success()) {
    scoped_write_.reset();
    return nullptr;
  }
  auto& sk_surface = sk_surfaces_[scoped_write_->image_index()];

  if (!sk_surface) {
    SkSurfaceProps surface_props =
        SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);
    const auto surface_format = vulkan_surface_->surface_format().format;
    DCHECK(surface_format == VK_FORMAT_B8G8R8A8_UNORM ||
           surface_format == VK_FORMAT_R8G8B8A8_UNORM);
    GrVkImageInfo vk_image_info(
        scoped_write_->image(), GrVkAlloc(), VK_IMAGE_TILING_OPTIMAL,
        scoped_write_->image_layout(), surface_format, 1 /* level_count */,
        VK_QUEUE_FAMILY_IGNORED,
        vulkan_surface_->swap_chain()->use_protected_memory()
            ? GrProtected::kYes
            : GrProtected::kNo);
    const auto& vk_image_size = vulkan_surface_->image_size();
    GrBackendRenderTarget render_target(vk_image_size.width(),
                                        vk_image_size.height(),
                                        0 /* sample_cnt */, vk_image_info);
    auto sk_color_type = surface_format == VK_FORMAT_B8G8R8A8_UNORM
                             ? kBGRA_8888_SkColorType
                             : kRGBA_8888_SkColorType;
    sk_surface = SkSurface::MakeFromBackendRenderTarget(
        context_provider_->GetGrContext(), render_target,
        kTopLeft_GrSurfaceOrigin, sk_color_type, sk_color_space_,
        &surface_props);
    DCHECK(sk_surface);
  } else {
    auto backend = sk_surface->getBackendRenderTarget(
        SkSurface::kFlushRead_BackendHandleAccess);
    backend.setVkImageLayout(scoped_write_->image_layout());
  }
  VkSemaphore vk_semaphore = scoped_write_->TakeBeginSemaphore();
  if (vk_semaphore != VK_NULL_HANDLE) {
    GrBackendSemaphore semaphore;
    semaphore.initVulkan(vk_semaphore);
    auto result = sk_surface->wait(1, &semaphore);
    DCHECK(result);
  }
  return sk_surface.get();
}

void SkiaOutputDeviceVulkan::EndPaint(const GrBackendSemaphore& semaphore) {
  DCHECK(scoped_write_);

  auto& sk_surface = sk_surfaces_[scoped_write_->image_index()];
  auto backend = sk_surface->getBackendRenderTarget(
      SkSurface::kFlushRead_BackendHandleAccess);
  GrVkImageInfo vk_image_info;
  if (!backend.getVkImageInfo(&vk_image_info))
    NOTREACHED() << "Failed to get the image info.";
  scoped_write_->set_image_layout(vk_image_info.fImageLayout);
  if (semaphore.isInitialized())
    scoped_write_->SetEndSemaphore(semaphore.vkSemaphore());
  scoped_write_.reset();
}

void SkiaOutputDeviceVulkan::CreateVulkanSurface() {
  gfx::AcceleratedWidget accelerated_widget = gfx::kNullAcceleratedWidget;
#if defined(OS_ANDROID)
  bool can_be_used_with_surface_control = false;
  accelerated_widget =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
          surface_handle_, &can_be_used_with_surface_control);
#else
  accelerated_widget = surface_handle_;
#endif
  auto vulkan_surface =
      context_provider_->GetVulkanImplementation()->CreateViewSurface(
          accelerated_widget);
  if (!vulkan_surface)
    LOG(FATAL) << "Failed to create vulkan surface.";
  if (!vulkan_surface->Initialize(context_provider_->GetDeviceQueue(),
                                  gpu::VulkanSurface::FORMAT_RGBA_32)) {
    LOG(FATAL) << "Failed to initialize vulkan surface.";
  }
  vulkan_surface_ = std::move(vulkan_surface);
}

}  // namespace viz
