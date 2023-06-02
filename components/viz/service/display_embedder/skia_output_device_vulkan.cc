// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurfaceMutableState.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrRecordingContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#include "ui/gfx/presentation_feedback.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/scoped_java_surface.h"
#endif

namespace viz {

// static
std::unique_ptr<SkiaOutputDeviceVulkan> SkiaOutputDeviceVulkan::Create(
    VulkanContextProvider* context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback) {
  auto output_device = std::make_unique<SkiaOutputDeviceVulkan>(
      base::PassKey<SkiaOutputDeviceVulkan>(), context_provider, surface_handle,
      memory_tracker, did_swap_buffer_complete_callback);
  if (UNLIKELY(!output_device->Initialize()))
    return nullptr;
  return output_device;
}

SkiaOutputDeviceVulkan::SkiaOutputDeviceVulkan(
    base::PassKey<SkiaOutputDeviceVulkan>,
    VulkanContextProvider* context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_provider->GetGrContext(),
                       /*graphite_context=*/nullptr,
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      context_provider_(context_provider),
      surface_handle_(surface_handle) {}

SkiaOutputDeviceVulkan::~SkiaOutputDeviceVulkan() {
  DCHECK(!scoped_write_);

  for (const auto& sk_surface_size_pair : sk_surface_size_pairs_) {
    memory_type_tracker_->TrackMemFree(sk_surface_size_pair.bytes_allocated);
  }
  sk_surface_size_pairs_.clear();

  if (UNLIKELY(!vulkan_surface_))
    return;

  auto* fence_helper = context_provider_->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
      std::move(vulkan_surface_));
}

#if BUILDFLAG(IS_WIN)
gpu::SurfaceHandle SkiaOutputDeviceVulkan::GetChildSurfaceHandle() {
  if (LIKELY(vulkan_surface_->accelerated_widget() != surface_handle_))
    return vulkan_surface_->accelerated_widget();
  return gpu::kNullSurfaceHandle;
}
#endif

bool SkiaOutputDeviceVulkan::Reshape(const SkImageInfo& image_info,
                                     const gfx::ColorSpace& color_space,
                                     int sample_count,
                                     float device_scale_factor,
                                     gfx::OverlayTransform transform) {
  DCHECK(!scoped_write_);

  if (UNLIKELY(!vulkan_surface_))
    return false;

  return RecreateSwapChain(image_info, sample_count, transform);
}

void SkiaOutputDeviceVulkan::Submit(bool sync_cpu, base::OnceClosure callback) {
  if (LIKELY(scoped_write_)) {
    auto& sk_surface =
        sk_surface_size_pairs_[scoped_write_->image_index()].sk_surface;
    DCHECK(sk_surface);
    auto queue_index =
        context_provider_->GetDeviceQueue()->GetVulkanQueueIndex();
    GrBackendSurfaceMutableState state(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                       queue_index);
    if (GrDirectContext* direct_context =
            GrAsDirectContext(sk_surface->recordingContext())) {
      direct_context->flush(sk_surface, {}, &state);
    }
  }

  SkiaOutputDevice::Submit(sync_cpu, std::move(callback));
}

void SkiaOutputDeviceVulkan::Present(
    const absl::optional<gfx::Rect>& update_rect,
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
  gfx::Rect rect =
      update_rect.value_or(gfx::Rect(vulkan_surface_->image_size()));
  // Reshape should have been called first.
  DCHECK(vulkan_surface_);
  DCHECK(!scoped_write_);
#if DCHECK_IS_ON()
  DCHECK_EQ(!rect.IsEmpty(), image_modified_);
  image_modified_ = false;
#endif

  StartSwapBuffers({});

  if (UNLIKELY(is_new_swap_chain_ &&
               rect == gfx::Rect(vulkan_surface_->image_size()))) {
    is_new_swap_chain_ = false;
  }

  if (LIKELY(!is_new_swap_chain_)) {
    auto image_index = vulkan_surface_->swap_chain()->current_image_index();
    for (size_t i = 0; i < damage_of_images_.size(); ++i) {
      if (UNLIKELY(i == image_index)) {
        damage_of_images_[i] = gfx::Rect();
      } else {
        damage_of_images_[i].Union(rect);
      }
    }
  }

  if (LIKELY(!rect.IsEmpty())) {
    // If the swapchain is new created, but rect doesn't cover the whole buffer,
    // we will still present it even it causes a artifact in this frame and
    // recovered when the next frame is presented. We do that because the old
    // swapchain's present thread is blocked on waiting a reply from xserver,
    // and presenting a new image with the new create swapchain will somehow
    // makes xserver send a reply to us, and then unblock the old swapchain's
    // present thread. So the old swapchain can be destroyed properly.
    vulkan_surface_->PostSubBufferAsync(
        rect,
        base::BindOnce(&SkiaOutputDeviceVulkan::OnPostSubBufferFinished,
                       weak_ptr_factory_.GetWeakPtr(), std::move(frame)),
        std::move(feedback));
  } else {
    OnPostSubBufferFinished(std::move(frame), gfx::SwapResult::SWAP_ACK);
    std::move(feedback).Run(gfx::PresentationFeedback(
        base::TimeTicks::Now(), vulkan_surface_->GetDisplayRefreshInterval(),
        0));
  }
}

SkSurface* SkiaOutputDeviceVulkan::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(vulkan_surface_);
  DCHECK(!scoped_write_);

  gpu::VulkanSwapChain::ScopedWrite scoped_write(vulkan_surface_->swap_chain());
  if (UNLIKELY(!scoped_write.success())) {
    // Return nullptr, and then the caller will make context lost.
    return nullptr;
  }

  auto& sk_surface =
      sk_surface_size_pairs_[scoped_write.image_index()].sk_surface;

  if (UNLIKELY(!sk_surface)) {
    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
    const auto surface_format = vulkan_surface_->surface_format().format;
    DCHECK(surface_format == VK_FORMAT_B8G8R8A8_UNORM ||
           surface_format == VK_FORMAT_R8G8B8A8_UNORM);
    GrVkImageInfo vk_image_info;
    vk_image_info.fImage = scoped_write.image();
    vk_image_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    vk_image_info.fImageLayout = scoped_write.image_layout();
    vk_image_info.fFormat = surface_format;
    vk_image_info.fImageUsageFlags = scoped_write.image_usage();
    vk_image_info.fSampleCount = 1;
    vk_image_info.fLevelCount = 1;
    vk_image_info.fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    vk_image_info.fProtected = GrProtected::kNo;
    const auto& vk_image_size = vulkan_surface_->image_size();
    GrBackendTexture backend_texture(vk_image_size.width(),
                                     vk_image_size.height(), vk_image_info);

    // Estimate size of GPU memory needed for the GrBackendRenderTarget.
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(
        context_provider_->GetDeviceQueue()->GetVulkanDevice(),
        vk_image_info.fImage, &requirements);
    sk_surface_size_pairs_[scoped_write.image_index()].bytes_allocated =
        requirements.size;
    memory_type_tracker_->TrackMemAlloc(requirements.size);
    sk_surface = SkSurfaces::WrapBackendTexture(
        context_provider_->GetGrContext(), backend_texture,
        kTopLeft_GrSurfaceOrigin, sample_count_, color_type_, color_space_,
        &surface_props);
    if (UNLIKELY(!sk_surface)) {
      return nullptr;
    }
  } else {
    auto backend = SkSurfaces::GetBackendRenderTarget(
        sk_surface.get(), SkSurfaces::BackendHandleAccess::kFlushRead);
    backend.setVkImageLayout(scoped_write.image_layout());
  }

  VkSemaphore vk_semaphore = scoped_write.begin_semaphore();
  DCHECK(vk_semaphore != VK_NULL_HANDLE);
  GrBackendSemaphore semaphore;
  semaphore.initVulkan(vk_semaphore);
  auto result =
      sk_surface->wait(1, &semaphore, /*deleteSemaphoresAfterWait=*/false);
  if (UNLIKELY(!result)) {
    return nullptr;
  }

  DCHECK(scoped_write.end_semaphore() != VK_NULL_HANDLE);
  GrBackendSemaphore end_semaphore;
  end_semaphore.initVulkan(scoped_write.end_semaphore());
  end_semaphores->push_back(std::move(end_semaphore));

  scoped_write_ = std::move(scoped_write);
  return sk_surface.get();
}

void SkiaOutputDeviceVulkan::EndPaint() {
  DCHECK(scoped_write_);

  auto& sk_surface =
      sk_surface_size_pairs_[scoped_write_->image_index()].sk_surface;
  auto backend = SkSurfaces::GetBackendRenderTarget(
        sk_surface.get(), SkSurfaces::BackendHandleAccess::kFlushRead);
#if DCHECK_IS_ON()
  GrVkImageInfo vk_image_info;
  if (UNLIKELY(!context_provider_->GetGrContext()->abandoned() &&
               !backend.getVkImageInfo(&vk_image_info))) {
    NOTREACHED() << "Failed to get the image info.";
  }
  DCHECK_EQ(vk_image_info.fImageLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
#endif
  scoped_write_.reset();
#if DCHECK_IS_ON()
  image_modified_ = true;
#endif
}

bool SkiaOutputDeviceVulkan::Initialize() {
  gfx::AcceleratedWidget accelerated_widget = gfx::kNullAcceleratedWidget;
#if BUILDFLAG(IS_ANDROID)
  bool can_be_used_with_surface_control = false;
  auto surface_variant =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(
          surface_handle_, &can_be_used_with_surface_control);
  // Should only reach here if surface control is disabled. In which case
  // browser should not be sending ScopedJavaSurfaceControl variant.
  CHECK(absl::holds_alternative<gl::ScopedJavaSurface>(surface_variant));
  gl::ScopedJavaSurface& scoped_java_surface =
      absl::get<gl::ScopedJavaSurface>(surface_variant);
  gl::ScopedANativeWindow window(scoped_java_surface);
  accelerated_widget = window.a_native_window();
#else
  accelerated_widget = surface_handle_;
#endif
  auto vulkan_surface =
      context_provider_->GetVulkanImplementation()->CreateViewSurface(
          accelerated_widget);
  if (UNLIKELY(!vulkan_surface)) {
    LOG(ERROR) << "Failed to create vulkan surface.";
    return false;
  }
  auto result = vulkan_surface->Initialize(context_provider_->GetDeviceQueue(),
                                           gpu::VulkanSurface::FORMAT_RGBA_32);
  if (UNLIKELY(!result)) {
    LOG(ERROR) << "Failed to initialize vulkan surface.";
    vulkan_surface->Destroy();
    return false;
  }
  vulkan_surface_ = std::move(vulkan_surface);

  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.pending_swap_params.max_pending_swaps = 1;
  // Vulkan FIFO swap chain should return vk images in presenting order, so set
  // preserve_buffer_content & supports_post_sub_buffer to true to let
  // SkiaOutputBufferImpl to manager damages.
  capabilities_.preserve_buffer_content = true;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities_.supports_post_sub_buffer = true;
  capabilities_.supports_target_damage = true;
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kHardware;
#if BUILDFLAG(IS_ANDROID)
  // With vulkan, if the chrome is launched in landscape mode, the chrome is
  // always blank until chrome window is rotated once. Workaround this problem
  // by using logic rotation mode.
  // TODO(https://crbug.com/1115065): use hardware orientation mode for vulkan,
  if (features::IsUsingVulkan())
    capabilities_.orientation_mode = OutputSurface::OrientationMode::kLogic;
#endif
  // We don't know the number of buffers until the VulkanSwapChain is
  // initialized, so set it to 0. Since |damage_area_from_skia_output_device| is
  // assigned to true, so |number_of_buffers| will not be used for tracking
  // framebuffer damages.
  capabilities_.number_of_buffers = 0;
  capabilities_.damage_area_from_skia_output_device = true;

  const auto surface_format = vulkan_surface_->surface_format().format;
  DCHECK(surface_format == VK_FORMAT_B8G8R8A8_UNORM ||
         surface_format == VK_FORMAT_R8G8B8A8_UNORM);

  auto sk_color_type = surface_format == VK_FORMAT_R8G8B8A8_UNORM
                           ? kRGBA_8888_SkColorType
                           : kBGRA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      sk_color_type;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      sk_color_type;
  // BGRX_8888 is used on Windows.
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      sk_color_type;
  return true;
}

bool SkiaOutputDeviceVulkan::RecreateSwapChain(
    const SkImageInfo& image_info,
    int sample_count,
    gfx::OverlayTransform transform) {
  auto generation = vulkan_surface_->swap_chain_generation();

  // Call vulkan_surface_->Reshape() will recreate vulkan swapchain if it is
  // necessary.
  if (UNLIKELY(!vulkan_surface_->Reshape(
          gfx::SkISizeToSize(image_info.dimensions()), transform))) {
    return false;
  }

  bool recreate =
      vulkan_surface_->swap_chain_generation() != generation ||
      !SkColorSpace::Equals(image_info.colorSpace(), color_space_.get()) ||
      sample_count_ != sample_count;
  if (LIKELY(recreate)) {
    // swapchain is changed, we need recreate all cached sk surfaces.
    for (const auto& sk_surface_size_pair : sk_surface_size_pairs_) {
      memory_type_tracker_->TrackMemFree(sk_surface_size_pair.bytes_allocated);
    }
    auto num_images = vulkan_surface_->swap_chain()->num_images();
    sk_surface_size_pairs_.clear();
    sk_surface_size_pairs_.resize(num_images);
    color_type_ = image_info.colorType();
    color_space_ = image_info.refColorSpace();
    sample_count_ = sample_count;
    damage_of_images_.resize(num_images);
    for (auto& damage : damage_of_images_)
      damage = gfx::Rect(vulkan_surface_->image_size());
    is_new_swap_chain_ = true;
  }

  return true;
}

void SkiaOutputDeviceVulkan::OnPostSubBufferFinished(OutputSurfaceFrame frame,
                                                     gfx::SwapResult result) {
  if (LIKELY(result == gfx::SwapResult::SWAP_ACK)) {
    auto image_index = vulkan_surface_->swap_chain()->current_image_index();
    FinishSwapBuffers(gfx::SwapCompletionResult(result),
                      vulkan_surface_->image_size(), std::move(frame),
                      damage_of_images_[image_index]);
  } else {
    FinishSwapBuffers(gfx::SwapCompletionResult(result),
                      vulkan_surface_->image_size(), std::move(frame),
                      gfx::Rect(vulkan_surface_->image_size()));
  }
}

SkiaOutputDeviceVulkan::SkSurfaceSizePair::SkSurfaceSizePair() = default;
SkiaOutputDeviceVulkan::SkSurfaceSizePair::SkSurfaceSizePair(
    const SkSurfaceSizePair& other) = default;
SkiaOutputDeviceVulkan::SkSurfaceSizePair::~SkSurfaceSizePair() = default;

}  // namespace viz
