// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_fuchsia.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/external_semaphore_pool.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace viz {

namespace {

void GrSemaphoresToGpuFenceHandles(
    gpu::VulkanImplementation* vulkan_implementation,
    VkDevice vk_device,
    const std::vector<GrBackendSemaphore>& semaphores,
    std::vector<gfx::GpuFenceHandle>& fence_handles) {
  for (auto& semaphore : semaphores) {
    gpu::SemaphoreHandle handle = vulkan_implementation->GetSemaphoreHandle(
        vk_device, semaphore.vkSemaphore());
    DCHECK(handle.is_valid());

    fence_handles.emplace_back();
    fence_handles.back().owned_event = zx::event(handle.TakeHandle());
  }
}

class PresenterImageFuchsia : public OutputPresenter::Image {
 public:
  explicit PresenterImageFuchsia(
      scoped_refptr<gfx::NativePixmap> native_pixmap);
  ~PresenterImageFuchsia() override;

  void BeginPresent() final;
  void EndPresent(gfx::GpuFenceHandle release_fence) final;
  int GetPresentCount() const final;
  void OnContextLost() final;

  const scoped_refptr<gfx::NativePixmap>& native_pixmap() const {
    return native_pixmap_;
  }

  void TakeSemaphores(std::vector<GrBackendSemaphore>& read_begin_semaphores,
                      std::vector<GrBackendSemaphore>& read_end_semaphores);

 private:
  scoped_refptr<gfx::NativePixmap> native_pixmap_;

  int present_count_ = 0;

  std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedReadAccess>
      read_access_;

  std::vector<GrBackendSemaphore> read_begin_semaphores_;
  std::vector<GrBackendSemaphore> read_end_semaphores_;
};

PresenterImageFuchsia::PresenterImageFuchsia(
    scoped_refptr<gfx::NativePixmap> native_pixmap)
    : native_pixmap_(std::move(native_pixmap)) {
  DCHECK(native_pixmap_);
}

PresenterImageFuchsia::~PresenterImageFuchsia() {
  DCHECK(read_begin_semaphores_.empty());
  DCHECK(read_end_semaphores_.empty());
}

void PresenterImageFuchsia::BeginPresent() {
  ++present_count_;

  if (present_count_ == 1) {
    DCHECK(!read_access_);
    DCHECK(read_begin_semaphores_.empty());
    DCHECK(read_end_semaphores_.empty());
    read_access_ = skia_representation()->BeginScopedReadAccess(
        &read_begin_semaphores_, &read_end_semaphores_);
  }
}

void PresenterImageFuchsia::EndPresent(gfx::GpuFenceHandle release_fence) {
  DCHECK(present_count_);
  DCHECK(release_fence.is_null());
  --present_count_;
  if (!present_count_)
    read_access_.reset();
}

int PresenterImageFuchsia::GetPresentCount() const {
  return present_count_;
}

void PresenterImageFuchsia::OnContextLost() {
  // Nothing to do here.
}

void PresenterImageFuchsia::TakeSemaphores(
    std::vector<GrBackendSemaphore>& read_begin_semaphores,
    std::vector<GrBackendSemaphore>& read_end_semaphores) {
  DCHECK(read_begin_semaphores.empty());
  std::swap(read_begin_semaphores, read_begin_semaphores_);

  DCHECK(read_end_semaphores.empty());
  std::swap(read_end_semaphores, read_end_semaphores_);
}

}  // namespace

OutputPresenterFuchsia::PendingFrame::PendingFrame() = default;
OutputPresenterFuchsia::PendingFrame::~PendingFrame() = default;

OutputPresenterFuchsia::PendingFrame::PendingFrame(PendingFrame&&) = default;
OutputPresenterFuchsia::PendingFrame&
OutputPresenterFuchsia::PendingFrame::operator=(PendingFrame&&) = default;

// static
std::unique_ptr<OutputPresenterFuchsia> OutputPresenterFuchsia::Create(
    ui::PlatformWindowSurface* window_surface,
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageFactory* shared_image_factory,
    gpu::SharedImageRepresentationFactory* representation_factory) {
  if (!base::FeatureList::IsEnabled(
          features::kUseSkiaOutputDeviceBufferQueue)) {
    return {};
  }

  return std::make_unique<OutputPresenterFuchsia>(
      window_surface, deps, shared_image_factory, representation_factory);
}

OutputPresenterFuchsia::OutputPresenterFuchsia(
    ui::PlatformWindowSurface* window_surface,
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageFactory* shared_image_factory,
    gpu::SharedImageRepresentationFactory* representation_factory)
    : window_surface_(window_surface),
      dependency_(deps),
      shared_image_factory_(shared_image_factory),
      shared_image_representation_factory_(representation_factory) {
  DCHECK(window_surface_);
}

OutputPresenterFuchsia::~OutputPresenterFuchsia() = default;

void OutputPresenterFuchsia::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities->supports_post_sub_buffer = false;
  capabilities->supports_commit_overlay_planes = false;
  capabilities->supports_surfaceless = true;

  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kRGBA_8888_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kRGBA_8888_SkColorType;
}

bool OutputPresenterFuchsia::Reshape(const gfx::Size& size,
                                     float device_scale_factor,
                                     const gfx::ColorSpace& color_space,
                                     gfx::BufferFormat format,
                                     gfx::OverlayTransform transform) {
  frame_size_ = size;
  return true;
}

std::vector<std::unique_ptr<OutputPresenter::Image>>
OutputPresenterFuchsia::AllocateImages(gfx::ColorSpace color_space,
                                       gfx::Size image_size,
                                       size_t num_images) {
  // Fuchsia allocates images in batches and does not support allocating and
  // releasing images on demand.
  CHECK_NE(num_images, 1u);

  // Create PresenterImageFuchsia for each buffer in the collection.
  constexpr uint32_t image_usage = gpu::SHARED_IMAGE_USAGE_DISPLAY |
                                   gpu::SHARED_IMAGE_USAGE_RASTER |
                                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  std::vector<std::unique_ptr<OutputPresenter::Image>> images;
  images.reserve(num_images);

  auto* surface_factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();

  VkDevice vk_device = dependency_->GetVulkanContextProvider()
                           ->GetDeviceQueue()
                           ->GetVulkanDevice();

  // Create an image for each buffer in the collection.
  for (size_t i = 0; i < num_images; ++i) {
    auto pixmap = surface_factory->CreateNativePixmap(
        dependency_->GetSurfaceHandle(), vk_device, frame_size_, buffer_format_,
        gfx::BufferUsage::SCANOUT);
    if (!pixmap)
      return {};

    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle = pixmap->ExportHandle();

    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    if (!shared_image_factory_->CreateSharedImage(
            mailbox, gpu::kDisplayCompositorClientId, std::move(gmb_handle),
            buffer_format_, gfx::BufferPlane::DEFAULT, gpu::kNullSurfaceHandle,
            frame_size_, color_space, kTopLeft_GrSurfaceOrigin,
            kPremul_SkAlphaType, image_usage)) {
      return {};
    }

    // There is a different NativePixmap object created inside of the shared
    // image backing from the cloned handle. While the two NativePixmap that
    // exist at this point should be equivalent, aka have the same handles,
    // store the new one and let the original be destroyed to avoid confusion.
    auto shared_image_pixmap =
        dependency_->GetSharedImageManager()->GetNativePixmap(mailbox);
    pixmap.reset();

    auto image =
        std::make_unique<PresenterImageFuchsia>(std::move(shared_image_pixmap));
    if (!image->Initialize(shared_image_factory_,
                           shared_image_representation_factory_, mailbox,
                           dependency_)) {
      return {};
    }
    images.push_back(std::move(image));
  }

  return images;
}

void OutputPresenterFuchsia::SwapBuffers(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  // SwapBuffer() should be called only after SchedulePrimaryPlane().
  DCHECK(next_frame_ && next_frame_->native_pixmap);

  next_frame_->completion_callback = std::move(completion_callback);
  next_frame_->presentation_callback = std::move(presentation_callback);

  PresentNextFrame();
}

void OutputPresenterFuchsia::PostSubBuffer(
    const gfx::Rect& rect,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  // Sub buffer presentation is not supported.
  NOTREACHED();
}

void OutputPresenterFuchsia::CommitOverlayPlanes(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  // Overlays are not supported yet.
  NOTREACHED();
}

void OutputPresenterFuchsia::SchedulePrimaryPlane(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
    Image* image,
    bool is_submitted) {
  auto* image_fuchsia = static_cast<PresenterImageFuchsia*>(image);

  if (!next_frame_)
    next_frame_.emplace();
  DCHECK(!next_frame_->native_pixmap);
  next_frame_->native_pixmap = image_fuchsia->native_pixmap();

  // Take semaphores for the image and covert them to zx::events that are later
  // passed to ImagePipe::PresentImage().
  std::vector<GrBackendSemaphore> read_begin_semaphores;
  std::vector<GrBackendSemaphore> read_end_semaphores;
  image_fuchsia->TakeSemaphores(read_begin_semaphores, read_end_semaphores);
  DCHECK(!read_begin_semaphores.empty());
  DCHECK(!read_end_semaphores.empty());

  auto* vulkan_context_provider = dependency_->GetVulkanContextProvider();
  auto* vulkan_implementation =
      vulkan_context_provider->GetVulkanImplementation();
  VkDevice vk_device =
      vulkan_context_provider->GetDeviceQueue()->GetVulkanDevice();

  GrSemaphoresToGpuFenceHandles(vulkan_implementation, vk_device,
                                read_begin_semaphores,
                                next_frame_->acquire_fences);
  GrSemaphoresToGpuFenceHandles(vulkan_implementation, vk_device,
                                read_end_semaphores,
                                next_frame_->release_fences);
}

void OutputPresenterFuchsia::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays,
    std::vector<ScopedOverlayAccess*> accesses) {
  if (!next_frame_)
    next_frame_.emplace();
  DCHECK(next_frame_->overlays.empty());

  for (auto& candidate : overlays) {
    DCHECK(candidate.mailbox.IsSharedImage());

    auto pixmap = dependency_->GetSharedImageManager()->GetNativePixmap(
        candidate.mailbox);
    if (!pixmap) {
      DLOG(ERROR) << "Cannot access SysmemNativePixmap";
      continue;
    }

    next_frame_->overlays.emplace_back();
    auto& overlay = next_frame_->overlays.back();
    overlay.pixmap = std::move(pixmap);
    overlay.overlay_plane_data = gfx::OverlayPlaneData(
        candidate.plane_z_order, candidate.transform,
        gfx::ToRoundedRect(candidate.display_rect), candidate.uv_rect,
        !candidate.is_opaque, gfx::ToRoundedRect(candidate.damage_rect),
        candidate.opacity, candidate.priority_hint, candidate.rounded_corners,
        candidate.color_space, candidate.hdr_metadata);
  }
}

void OutputPresenterFuchsia::PresentNextFrame() {
  DCHECK(next_frame_);

  PendingFrame& frame = next_frame_.value();
  window_surface_->Present(
      std::move(frame.native_pixmap), std::move(frame.overlays),
      std::move(frame.acquire_fences), std::move(frame.release_fences),
      std::move(frame.completion_callback),
      std::move(frame.presentation_callback));

  next_frame_.reset();
}

}  // namespace viz
