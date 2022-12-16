// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_fuchsia.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace viz {

namespace {

class PresenterImageFuchsia : public OutputPresenter::Image {
 public:
  PresenterImageFuchsia(
      gpu::SharedImageFactory* factory,
      gpu::SharedImageRepresentationFactory* representation_factory,
      SkiaOutputSurfaceDependency* deps)
      : Image(factory, representation_factory, deps) {}
  ~PresenterImageFuchsia() override;

  // OutputPresenter::Image implementation.
  void BeginPresent() final;
  void EndPresent(gfx::GpuFenceHandle release_fence) final;
  int GetPresentCount() const final;
  void OnContextLost() final;

  // Must only be called in between BeginPresent() and EndPresent().
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() {
    DCHECK(scoped_overlay_read_access_);
    return scoped_overlay_read_access_->GetNativePixmap();
  }

  // Must be called after BeginPresent() to get fences for frame.
  void TakePresentationFences(
      std::vector<gfx::GpuFenceHandle>& read_begin_fences,
      std::vector<gfx::GpuFenceHandle>& read_end_fences);

 private:
  std::vector<gfx::GpuFenceHandle> read_begin_fences_;
  gfx::GpuFenceHandle read_end_fence_;
};

PresenterImageFuchsia::~PresenterImageFuchsia() {
  DCHECK(read_begin_fences_.empty());
  DCHECK(read_end_fence_.is_null());
}

void PresenterImageFuchsia::BeginPresent() {
  DCHECK(read_end_fence_.is_null());

  ++present_count_;

  if (present_count_ == 1) {
    DCHECK(!scoped_overlay_read_access_);
    DCHECK(read_begin_fences_.empty());

    scoped_overlay_read_access_ =
        overlay_representation_->BeginScopedReadAccess();
    DCHECK(scoped_overlay_read_access_);

    // Take ownership of acquire fence.
    gfx::GpuFenceHandle gpu_fence_handle =
        scoped_overlay_read_access_->TakeAcquireFence();
    if (!gpu_fence_handle.is_null())
      read_begin_fences_.push_back(std::move(gpu_fence_handle));
  }

  // A new release fence is generated for each present. The fence for the last
  // present gets waited on before giving up read access to the shared image.
  gpu::ExternalSemaphore semaphore =
      gpu::ExternalSemaphore::Create(deps_->GetVulkanContextProvider());
  DCHECK(semaphore.is_valid());
  read_end_fence_ = semaphore.TakeSemaphoreHandle().ToGpuFenceHandle();

  scoped_overlay_read_access_->SetReleaseFence(read_end_fence_.Clone());
}

void PresenterImageFuchsia::EndPresent(gfx::GpuFenceHandle release_fence) {
  DCHECK(present_count_);
  DCHECK(release_fence.is_null());
  --present_count_;
  if (!present_count_) {
    DCHECK(scoped_overlay_read_access_);
    scoped_overlay_read_access_.reset();
  }
}

int PresenterImageFuchsia::GetPresentCount() const {
  return present_count_;
}

void PresenterImageFuchsia::OnContextLost() {
  if (overlay_representation_)
    overlay_representation_->OnContextLost();
}

void PresenterImageFuchsia::TakePresentationFences(
    std::vector<gfx::GpuFenceHandle>& read_begin_fences,
    std::vector<gfx::GpuFenceHandle>& read_end_fences) {
  DCHECK(read_begin_fences.empty());
  std::swap(read_begin_fences, read_begin_fences_);

  DCHECK(read_end_fences.empty());
  DCHECK(!read_end_fence_.is_null());
  read_end_fences.push_back(std::move(read_end_fence_));
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

bool OutputPresenterFuchsia::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  frame_size_ = gfx::SkISizeToSize(characterization.dimensions());
  return true;
}

std::vector<std::unique_ptr<OutputPresenter::Image>>
OutputPresenterFuchsia::AllocateImages(gfx::ColorSpace color_space,
                                       gfx::Size image_size,
                                       size_t num_images) {
  DCHECK(!features::ShouldRendererAllocateImages());

  // Fuchsia allocates images in batches and does not support allocating and
  // releasing images on demand.
  CHECK_NE(num_images, 1u);

  // Create PresenterImageFuchsia for each buffer in the collection.
  constexpr uint32_t image_usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE |
                                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  std::vector<std::unique_ptr<OutputPresenter::Image>> images;
  images.reserve(num_images);

  // Create an image for each buffer in the collection.
  for (size_t i = 0; i < num_images; ++i) {
    auto image = std::make_unique<PresenterImageFuchsia>(
        shared_image_factory_, shared_image_representation_factory_,
        dependency_);
    if (!image->Initialize(frame_size_, color_space, si_format_, image_usage)) {
      return {};
    }
    images.push_back(std::move(image));
  }

  return images;
}

void OutputPresenterFuchsia::SwapBuffers(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback,
    gfx::FrameData data) {
  // SwapBuffer() should be called only after SchedulePrimaryPlane().
  DCHECK(next_frame_ && next_frame_->native_pixmap);

  next_frame_->completion_callback = std::move(completion_callback);
  next_frame_->presentation_callback = std::move(presentation_callback);

  PresentNextFrame();
}

void OutputPresenterFuchsia::PostSubBuffer(
    const gfx::Rect& rect,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback,
    gfx::FrameData data) {
  // Sub buffer presentation is not supported.
  NOTREACHED();
}

void OutputPresenterFuchsia::CommitOverlayPlanes(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback,
    gfx::FrameData data) {
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
  next_frame_->native_pixmap = image_fuchsia->GetNativePixmap();

  // Take semaphores for the image to be passed to ImagePipe::PresentImage().
  image_fuchsia->TakePresentationFences(next_frame_->acquire_fences,
                                        next_frame_->release_fences);
  DCHECK(!next_frame_->acquire_fences.empty());
  DCHECK(!next_frame_->release_fences.empty());
}

void OutputPresenterFuchsia::ScheduleOverlayPlane(
    const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
    ScopedOverlayAccess* access,
    std::unique_ptr<gfx::GpuFence> acquire_fence) {
  // TODO(msisov): this acquire fence is only valid when tiles are rastered for
  // scanout usage, which are used for DelegatedCompositing in LaCros. It's not
  // expected to have this fence created for fuchsia. As soon as a better place
  // for this fence is found, this will be removed. For now, add a dcheck that
  // verifies the fence is null.
  DCHECK(!acquire_fence);

  if (!next_frame_)
    next_frame_.emplace();
  DCHECK(next_frame_->overlays.empty());

  DCHECK(overlay_plane_candidate.mailbox.IsSharedImage());
  auto pixmap = access ? access->GetNativePixmap() : nullptr;

  if (!pixmap) {
    DLOG(ERROR) << "Cannot access SysmemNativePixmap";
    return;
  }

  if (overlay_plane_candidate.is_root_render_pass) {
    DCHECK(features::ShouldRendererAllocateImages());
    DCHECK(!next_frame_->native_pixmap);
    next_frame_->native_pixmap = std::move(pixmap);

    // Pass the acquire fence to system compositor if one exists.
    gfx::GpuFenceHandle acqire_fence = access->TakeAcquireFence();
    if (!acqire_fence.is_null())
      next_frame_->acquire_fences.push_back(std::move(acqire_fence));

    // Create and pass a release fence to the system compositor too.
    gpu::ExternalSemaphore semaphore =
        gpu::ExternalSemaphore::Create(dependency_->GetVulkanContextProvider());
    DCHECK(semaphore.is_valid());
    auto release_fence = semaphore.TakeSemaphoreHandle().ToGpuFenceHandle();
    next_frame_->release_fences.push_back(release_fence.Clone());

    // The release fence is signaled when the primary plane buffer can be
    // reused, rather than after it's first presented, so added as release fence
    // for the current access directly.
    access->SetReleaseFence(std::move(release_fence));
  } else {
    next_frame_->overlays.emplace_back();
    auto& overlay = next_frame_->overlays.back();
    overlay.pixmap = std::move(pixmap);
    overlay.overlay_plane_data = gfx::OverlayPlaneData(
        overlay_plane_candidate.plane_z_order,
        absl::get<gfx::OverlayTransform>(overlay_plane_candidate.transform),
        overlay_plane_candidate.display_rect, overlay_plane_candidate.uv_rect,
        !overlay_plane_candidate.is_opaque,
        gfx::ToRoundedRect(overlay_plane_candidate.damage_rect),
        overlay_plane_candidate.opacity, overlay_plane_candidate.priority_hint,
        overlay_plane_candidate.rounded_corners,
        overlay_plane_candidate.color_space,
        overlay_plane_candidate.hdr_metadata);
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
