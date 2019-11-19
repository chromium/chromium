// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gl/color_space_utils.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"

namespace viz {

static constexpr uint32_t kSharedImageUsage =
    gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY |
    gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;

class SkiaOutputDeviceBufferQueue::Image {
 public:
  Image(gpu::SharedImageFactory* factory,
        gpu::SharedImageRepresentationFactory* representation_factory);
  ~Image();

  bool Initialize(const gfx::Size& size,
                  const gfx::ColorSpace& color_space,
                  ResourceFormat format,
                  SkiaOutputSurfaceDependency* deps,
                  uint32_t shared_image_usage);

  SkSurface* BeginWriteSkia();
  void EndWriteSkia();
  void BeginPresent();
  void EndPresent();
  gl::GLImage* GetGLImage() const;
  std::unique_ptr<gfx::GpuFence> CreateFence();
  void ResetFence();

 private:
  gpu::SharedImageFactory* const factory_;
  gpu::SharedImageRepresentationFactory* const representation_factory_;
  gpu::Mailbox mailbox_;

  std::unique_ptr<gpu::SharedImageRepresentationSkia> skia_representation_;
  std::unique_ptr<gpu::SharedImageRepresentationGLTexture> gl_representation_;
  base::Optional<gpu::SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access_;
  base::Optional<gpu::SharedImageRepresentationGLTexture::ScopedAccess>
      scoped_read_access_;
  std::vector<GrBackendSemaphore> end_semaphores_;
  std::unique_ptr<gl::GLFence> fence_;

  DISALLOW_COPY_AND_ASSIGN(Image);
};

SkiaOutputDeviceBufferQueue::Image::Image(
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory)
    : factory_(factory), representation_factory_(representation_factory) {}

SkiaOutputDeviceBufferQueue::Image::~Image() {
  scoped_read_access_.reset();
  scoped_write_access_.reset();
  skia_representation_.reset();
  gl_representation_.reset();

  if (!mailbox_.IsZero())
    factory_->DestroySharedImage(mailbox_);
}

bool SkiaOutputDeviceBufferQueue::Image::Initialize(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    ResourceFormat format,
    SkiaOutputSurfaceDependency* deps,
    uint32_t shared_image_usage) {
  mailbox_ = gpu::Mailbox::GenerateForSharedImage();
  if (factory_->CreateSharedImage(mailbox_, format, size, color_space,
                                  shared_image_usage)) {
    skia_representation_ = representation_factory_->ProduceSkia(
        mailbox_, deps->GetSharedContextState());
    gl_representation_ = representation_factory_->ProduceGLTexture(mailbox_);

    return true;
  }

  mailbox_.SetZero();
  return false;
}

SkSurface* SkiaOutputDeviceBufferQueue::Image::BeginWriteSkia() {
  DCHECK(!scoped_write_access_);
  DCHECK(!scoped_read_access_);
  DCHECK(end_semaphores_.empty());

  std::vector<GrBackendSemaphore> begin_semaphores;
  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

  // TODO(vasilyt): Props and MSAA
  scoped_write_access_.emplace(skia_representation_.get(),
                               0 /* final_msaa_count */, surface_props,
                               &begin_semaphores, &end_semaphores_);
  DCHECK(scoped_write_access_->success());
  if (!begin_semaphores.empty()) {
    scoped_write_access_->surface()->wait(begin_semaphores.size(),
                                          begin_semaphores.data());
  }

  return scoped_write_access_->surface();
}

void SkiaOutputDeviceBufferQueue::Image::EndWriteSkia() {
  DCHECK(scoped_write_access_);
  GrFlushInfo flush_info = {
      .fFlags = kNone_GrFlushFlags,
      .fNumSemaphores = end_semaphores_.size(),
      .fSignalSemaphores = end_semaphores_.data(),
  };
  scoped_write_access_->surface()->flush(
      SkSurface::BackendSurfaceAccess::kPresent, flush_info);
  scoped_write_access_.reset();
  end_semaphores_.clear();
}

void SkiaOutputDeviceBufferQueue::Image::BeginPresent() {
  DCHECK(!scoped_write_access_);
  DCHECK(!scoped_read_access_);
  scoped_read_access_.emplace(gl_representation_.get(),
                              GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
}

void SkiaOutputDeviceBufferQueue::Image::EndPresent() {
  DCHECK(scoped_read_access_);
  scoped_read_access_.reset();
  // If the GpuFence was created for ScheduleOverlayPlane we can release it now.
  fence_.reset();
  return;
}

gl::GLImage* SkiaOutputDeviceBufferQueue::Image::GetGLImage() const {
  auto* texture = gl_representation_->GetTexture();
  return texture->GetLevelImage(texture->target(), 0);
}

std::unique_ptr<gfx::GpuFence>
SkiaOutputDeviceBufferQueue::Image::CreateFence() {
  if (!fence_)
    fence_ = gl::GLFence::CreateForGpuFence();
  return fence_->GetGpuFence();
}

SkiaOutputDeviceBufferQueue::SkiaOutputDeviceBufferQueue(
    scoped_refptr<gl::GLSurface> gl_surface,
    SkiaOutputSurfaceDependency* deps,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    uint32_t shared_image_usage)
    : SkiaOutputDevice(false /*need_swap_semaphore */,
                       did_swap_buffer_complete_callback),
      dependency_(deps),
      gl_surface_(gl_surface),
      shared_image_factory_(deps->GetGpuPreferences(),
                            deps->GetGpuDriverBugWorkarounds(),
                            deps->GetGpuFeatureInfo(),
                            deps->GetSharedContextState().get(),
                            deps->GetMailboxManager(),
                            deps->GetSharedImageManager(),
                            deps->GetGpuImageFactory(),
                            nullptr,
                            true),
      shared_image_usage_(shared_image_usage) {
  shared_image_representation_factory_ =
      std::make_unique<gpu::SharedImageRepresentationFactory>(
          deps->GetSharedImageManager(), nullptr);

#if defined(USE_OZONE)
  image_format_ = GetResourceFormat(display::DisplaySnapshot::PrimaryFormat());
#else
  image_format_ = RGBA_8888;
#endif

  // TODO(vasilyt): Need to figure out why partial swap isn't working
  capabilities_.supports_post_sub_buffer = false;
  capabilities_.max_frames_pending = 2;
}

SkiaOutputDeviceBufferQueue::SkiaOutputDeviceBufferQueue(
    scoped_refptr<gl::GLSurface> gl_surface,
    SkiaOutputSurfaceDependency* deps,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback)
    : SkiaOutputDeviceBufferQueue(gl_surface,
                                  deps,
                                  did_swap_buffer_complete_callback,
                                  kSharedImageUsage) {}

SkiaOutputDeviceBufferQueue::~SkiaOutputDeviceBufferQueue() {
  FreeAllSurfaces();
}

SkiaOutputDeviceBufferQueue::Image*
SkiaOutputDeviceBufferQueue::GetCurrentImage() {
  if (!current_image_)
    current_image_ = GetNextImage();

  return current_image_.get();
}

std::unique_ptr<SkiaOutputDeviceBufferQueue::Image>
SkiaOutputDeviceBufferQueue::GetNextImage() {
  if (!available_images_.empty()) {
    std::unique_ptr<Image> image = std::move(available_images_.back());
    available_images_.pop_back();
    return image;
  }

  auto image = std::make_unique<Image>(
      &shared_image_factory_, shared_image_representation_factory_.get());

  if (image->Initialize(image_size_, color_space_, image_format_, dependency_,
                        shared_image_usage_)) {
    return image;
  }

  return nullptr;
}

void SkiaOutputDeviceBufferQueue::PageFlipComplete() {
  DCHECK(!in_flight_images_.empty());

  if (in_flight_images_.front()) {
    if (displayed_image_)
      available_images_.push_back(std::move(displayed_image_));
    displayed_image_ = std::move(in_flight_images_.front());
    if (displayed_image_)
      displayed_image_->EndPresent();
  }

  in_flight_images_.pop_front();
}

void SkiaOutputDeviceBufferQueue::FreeAllSurfaces() {
  displayed_image_.reset();
  current_image_.reset();
  // This is intentionally not emptied since the swap buffers acks are still
  // expected to arrive.
  for (auto& image : in_flight_images_)
    image = nullptr;

  available_images_.clear();
}

gl::GLImage* SkiaOutputDeviceBufferQueue::GetOverlayImage() {
  if (current_image_)
    return current_image_->GetGLImage();
  return nullptr;
}

std::unique_ptr<gfx::GpuFence>
SkiaOutputDeviceBufferQueue::SubmitOverlayGpuFence() {
  if (current_image_) {
    return current_image_->CreateFence();
  }
  return nullptr;
}

void SkiaOutputDeviceBufferQueue::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  // BeginPain() is not called after last SwapBuffer(), if |current_image_| is
  // nullptr.
  if (current_image_)
    current_image_->BeginPresent();
  in_flight_images_.push_back(std::move(current_image_));

  StartSwapBuffers({});

  if (gl_surface_->SupportsAsyncSwap()) {
    auto callback = base::BindOnce(
        &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
        weak_ptr_factory_.GetWeakPtr(), image_size_, std::move(latency_info));
    gl_surface_->SwapBuffersAsync(std::move(callback), std::move(feedback));
  } else {
    DoFinishSwapBuffers(image_size_, std::move(latency_info),
                        gl_surface_->SwapBuffers(std::move(feedback)), nullptr);
  }
}

void SkiaOutputDeviceBufferQueue::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  in_flight_images_.push_back(std::move(current_image_));
  StartSwapBuffers({});

  if (gl_surface_->SupportsAsyncSwap()) {
    auto callback = base::BindOnce(
        &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
        weak_ptr_factory_.GetWeakPtr(), image_size_, std::move(latency_info));
    gl_surface_->PostSubBufferAsync(rect.x(), rect.y(), rect.width(),
                                    rect.height(), std::move(callback),
                                    std::move(feedback));

  } else {
    DoFinishSwapBuffers(
        image_size_, std::move(latency_info),
        gl_surface_->PostSubBuffer(rect.x(), rect.y(), rect.width(),
                                   rect.height(), std::move(feedback)),
        nullptr);
  }
}

void SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers(
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(!gpu_fence);

  PageFlipComplete();
  FinishSwapBuffers(result, size, latency_info);
}

bool SkiaOutputDeviceBufferQueue::Reshape(const gfx::Size& size,
                                          float device_scale_factor,
                                          const gfx::ColorSpace& color_space,
                                          bool has_alpha,
                                          gfx::OverlayTransform transform) {
  gl::GLSurface::ColorSpace surface_color_space =
      gl::ColorSpaceUtils::GetGLSurfaceColorSpace(color_space);
  if (!gl_surface_->Resize(size, device_scale_factor, surface_color_space,
                           has_alpha)) {
    DLOG(ERROR) << "Failed to resize.";
    return false;
  }

  color_space_ = color_space;
  image_size_ = size;
  FreeAllSurfaces();
  return true;
}

SkSurface* SkiaOutputDeviceBufferQueue::BeginPaint() {
  auto* image = GetCurrentImage();
  return image->BeginWriteSkia();
}
void SkiaOutputDeviceBufferQueue::EndPaint(
    const GrBackendSemaphore& semaphore) {
  auto* image = GetCurrentImage();
  image->EndWriteSkia();
}

}  // namespace viz
