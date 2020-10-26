// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gl/gl_surface.h"

namespace viz {

class SkiaOutputDeviceBufferQueue::OverlayData {
 public:
  OverlayData() = default;

  OverlayData(
      std::unique_ptr<gpu::SharedImageRepresentationOverlay> representation,
      std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
          scoped_read_access)
      : representation_(std::move(representation)),
        scoped_read_access_(std::move(scoped_read_access)),
        ref_(1) {
    DCHECK(representation_);
    DCHECK(scoped_read_access_);
  }

  OverlayData(OverlayData&& other) { *this = std::move(other); }

  ~OverlayData() { Reset(); }

  OverlayData& operator=(OverlayData&& other) {
    DCHECK(!IsInUseByWindowServer());
    DCHECK(!ref_);
    DCHECK(!scoped_read_access_);
    DCHECK(!representation_);
    scoped_read_access_ = std::move(other.scoped_read_access_);
    representation_ = std::move(other.representation_);
    ref_ = other.ref_;
    other.ref_ = 0;
    return *this;
  }

  bool IsInUseByWindowServer() const {
    if (!scoped_read_access_)
      return false;
    auto* gl_image = scoped_read_access_->gl_image();
    if (!gl_image)
      return false;
    return gl_image->IsInUseByWindowServer();
  }

  void Ref() { ++ref_; }

  void Unref() {
    DCHECK_GT(ref_, 0);
    if (ref_ > 1) {
      --ref_;
    } else if (ref_ == 1) {
      DCHECK(!IsInUseByWindowServer());
      Reset();
    }
  }

  bool unique() const { return ref_ == 1; }
  const gpu::Mailbox& mailbox() const { return representation_->mailbox(); }
  gpu::SharedImageRepresentationOverlay::ScopedReadAccess* scoped_read_access()
      const {
    return scoped_read_access_.get();
  }

 private:
  void Reset() {
    scoped_read_access_.reset();
    representation_.reset();
    ref_ = 0;
  }

  std::unique_ptr<gpu::SharedImageRepresentationOverlay> representation_;
  std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
      scoped_read_access_;
  int ref_ = 0;
};

SkiaOutputDeviceBufferQueue::SkiaOutputDeviceBufferQueue(
    std::unique_ptr<OutputPresenter> presenter,
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageRepresentationFactory* representation_factory,
    gpu::MemoryTracker* memory_tracker,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback)
    : SkiaOutputDevice(deps->GetSharedContextState()->gr_context(),
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      presenter_(std::move(presenter)),
      dependency_(deps),
      representation_factory_(representation_factory) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.preserve_buffer_content = true;
  capabilities_.only_invalidates_damage_rect = false;
  capabilities_.number_of_buffers = 3;
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kHardware;
#if defined(OS_ANDROID)
  // With vulkan, if the chrome is launched in landscape mode, the chrome is
  // always blank until chrome window is rotated once. Workaround this problem
  // by using logic rotation mode.
  // TODO(https://crbug.com/1115065): use hardware orientation mode for vulkan,
  if (dependency_->GetSharedContextState()->GrContextIsVulkan() &&
      base::FeatureList::GetFieldTrial(features::kVulkan)) {
    capabilities_.orientation_mode = OutputSurface::OrientationMode::kLogic;
  }
#endif

  // Force the number of max pending frames to one when the switch
  // "double-buffer-compositing" is passed.
  // This will keep compositing in double buffered mode assuming |buffer_queue|
  // allocates at most one additional buffer.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDoubleBufferCompositing))
    capabilities_.number_of_buffers = 2;
  capabilities_.max_frames_pending = capabilities_.number_of_buffers - 1;

  presenter_->InitializeCapabilities(&capabilities_);

  if (capabilities_.supports_post_sub_buffer)
    capabilities_.supports_target_damage = true;
}

SkiaOutputDeviceBufferQueue::~SkiaOutputDeviceBufferQueue() {
  FreeAllSurfaces();
  // Clear and cancel swap_completion_callbacks_ to free all resource bind to
  // callbacks.
  swap_completion_callbacks_.clear();
}

OutputPresenter::Image* SkiaOutputDeviceBufferQueue::GetNextImage() {
  DCHECK(!available_images_.empty());
  auto* image = available_images_.front();
  available_images_.pop_front();
  return image;
}

void SkiaOutputDeviceBufferQueue::PageFlipComplete(
    OutputPresenter::Image* image) {
  if (displayed_image_) {
    DCHECK_EQ(displayed_image_->skia_representation()->size(), image_size_);
    DCHECK_EQ(displayed_image_->present_count() > 1, displayed_image_ == image);
    displayed_image_->EndPresent();
    if (!displayed_image_->present_count()) {
      available_images_.push_back(displayed_image_);
      // Call BeginWriteSkia() for the next frame here to avoid some expensive
      // operations on the critical code path.
      auto shared_context_state = dependency_->GetSharedContextState();
      if (!available_images_.front()->sk_surface() &&
          shared_context_state->MakeCurrent(nullptr)) {
        // BeginWriteSkia() may alter GL's state.
        shared_context_state->set_need_context_state_reset(true);
        available_images_.front()->BeginWriteSkia();
      }
    }
  }

  displayed_image_ = image;
  swap_completion_callbacks_.pop_front();
}

void SkiaOutputDeviceBufferQueue::FreeAllSurfaces() {
  images_.clear();
  current_image_ = nullptr;
  submitted_image_ = nullptr;
  displayed_image_ = nullptr;
  available_images_.clear();
}

bool SkiaOutputDeviceBufferQueue::IsPrimaryPlaneOverlay() const {
  return true;
}

void SkiaOutputDeviceBufferQueue::SchedulePrimaryPlane(
    const base::Optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>&
        plane) {
  if (plane) {
    // If the current_image_ is nullptr, it means there is no change on the
    // primary plane. So we just need to schedule the last submitted image.
    auto* image = current_image_ ? current_image_ : submitted_image_;
    DCHECK(image);

    image->BeginPresent();
    presenter_->SchedulePrimaryPlane(plane.value(), image,
                                     image == submitted_image_);
  } else {
    current_frame_has_no_primary_plane_ = true;
  }
}

void SkiaOutputDeviceBufferQueue::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  DCHECK(pending_overlay_mailboxes_.empty());
  std::vector<OutputPresenter::ScopedOverlayAccess*> accesses(overlays.size());
  for (size_t i = 0; i < overlays.size(); ++i) {
    const auto& overlay = overlays[i];
    if (!overlay.mailbox.IsSharedImage())
      continue;

    auto it = overlays_.find(overlay.mailbox);
    if (it != overlays_.end()) {
      // If the overlay is in |overlays_|, we will reuse it, and a ref will be
      // added to keep it alive. This ref will be removed, when the overlay is
      // replaced by a new frame.
      it->Ref();
      accesses[i] = it->scoped_read_access();
      pending_overlay_mailboxes_.emplace_back(overlay.mailbox);
      continue;
    }

    auto shared_image =
        representation_factory_->ProduceOverlay(overlay.mailbox);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image) {
      LOG(ERROR) << "Invalid mailbox.";
      continue;
    }

    // TODO(penghuang): do not depend on GLImage.
    auto shared_image_access =
        shared_image->BeginScopedReadAccess(true /* needs_gl_image */);
    if (!shared_image_access) {
      LOG(ERROR) << "Could not access SharedImage for read.";
      continue;
    }

    // TODO(penghuang): do not depend on GLImage.
    DLOG_IF(FATAL, !shared_image_access->gl_image()) << "Cannot get GLImage.";

    bool result;
    std::tie(it, result) = overlays_.emplace(std::move(shared_image),
                                             std::move(shared_image_access));
    DCHECK(result);
    DCHECK(it->unique());

    // Add an extra ref to keep it alive. This extra ref will be removed when
    // the backing is not used by system compositor anymore.
    it->Ref();
    accesses[i] = it->scoped_read_access();
    pending_overlay_mailboxes_.emplace_back(overlay.mailbox);
  }

  presenter_->ScheduleOverlays(std::move(overlays), std::move(accesses));
}

void SkiaOutputDeviceBufferQueue::PreGrContextSubmit() {
  // The current image may be missing, for example during WebXR presentation.
  if (current_image_)
    current_image_->PreGrContextSubmit();
}

void SkiaOutputDeviceBufferQueue::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  if (current_frame_has_no_primary_plane_) {
    DCHECK(!current_image_);
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    DCHECK(current_image_);
    submitted_image_ = current_image_;
    current_image_ = nullptr;
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), GetSwapBuffersSize(), std::move(latency_info),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->SwapBuffers(swap_completion_callbacks_.back()->callback(),
                          std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  if (current_frame_has_no_primary_plane_) {
    DCHECK(!current_image_);
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    if (current_image_) {
      submitted_image_ = current_image_;
      current_image_ = nullptr;
    }
    DCHECK(submitted_image_);
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), GetSwapBuffersSize(), std::move(latency_info),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->PostSubBuffer(rect, swap_completion_callbacks_.back()->callback(),
                            std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  // There is no drawing for this frame on the main buffer.
  DCHECK(!current_image_);
  if (current_frame_has_no_primary_plane_) {
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    DCHECK(submitted_image_);
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), GetSwapBuffersSize(), std::move(latency_info),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->CommitOverlayPlanes(swap_completion_callbacks_.back()->callback(),
                                  std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers(
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    const base::WeakPtr<OutputPresenter::Image>& image,
    std::vector<gpu::Mailbox> overlay_mailboxes,
    gfx::SwapCompletionResult result) {
  // |overlay_mailboxes| are for overlays used by previous frame, they should
  // have been replaced.
  for (const auto& mailbox : overlay_mailboxes) {
    auto it = overlays_.find(mailbox);
    DCHECK(it != overlays_.end());
    it->Unref();
  }

  // Go through backings of all overlays, and release overlay backings which are
  // not used.
  std::vector<gpu::Mailbox> released_overlays;
  base::EraseIf(overlays_, [&released_overlays](auto& overlay) {
    if (!overlay.unique())
      return false;
    if (overlay.IsInUseByWindowServer())
      return false;
#if defined(OS_APPLE)
    // Right now, only macOS needs to return maliboxes of released overlays, so
    // SkiaRenderer can unlock resources for them.
    released_overlays.push_back(overlay.mailbox());
#else
    ALLOW_UNUSED_LOCAL(released_overlays);
#endif
    overlay.Unref();
    return true;
  });

  DCHECK(!result.gpu_fence);
  FinishSwapBuffers(std::move(result), size, latency_info,
                    /*damage_area=*/base::nullopt,
                    std::move(released_overlays));
  PageFlipComplete(image.get());
}

gfx::Size SkiaOutputDeviceBufferQueue::GetSwapBuffersSize() {
  switch (overlay_transform_) {
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return gfx::Size(image_size_.height(), image_size_.width());
    case gfx::OVERLAY_TRANSFORM_INVALID:
    case gfx::OVERLAY_TRANSFORM_NONE:
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return image_size_;
  }
}

bool SkiaOutputDeviceBufferQueue::Reshape(const gfx::Size& size,
                                          float device_scale_factor,
                                          const gfx::ColorSpace& color_space,
                                          gfx::BufferFormat format,
                                          gfx::OverlayTransform transform) {
  DCHECK(pending_overlay_mailboxes_.empty());
  if (!presenter_->Reshape(size, device_scale_factor, color_space, format,
                           transform)) {
    DLOG(ERROR) << "Failed to resize.";
    return false;
  }

  color_space_ = color_space;
  image_size_ = size;
  overlay_transform_ = transform;
  FreeAllSurfaces();

  images_ = presenter_->AllocateImages(color_space_, image_size_,
                                       capabilities_.number_of_buffers);
  if (images_.empty())
    return false;

  for (auto& image : images_) {
    available_images_.push_back(image.get());
  }

  return true;
}

SkSurface* SkiaOutputDeviceBufferQueue::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!current_image_)
    current_image_ = GetNextImage();
  if (!current_image_->sk_surface())
    current_image_->BeginWriteSkia();
  *end_semaphores = current_image_->TakeEndWriteSkiaSemaphores();
  return current_image_->sk_surface();
}

void SkiaOutputDeviceBufferQueue::EndPaint() {
  DCHECK(current_image_);
  current_image_->EndWriteSkia();
}

bool SkiaOutputDeviceBufferQueue::OverlayDataComparator::operator()(
    const OverlayData& lhs,
    const OverlayData& rhs) const {
  return lhs.mailbox() < rhs.mailbox();
}

bool SkiaOutputDeviceBufferQueue::OverlayDataComparator::operator()(
    const OverlayData& lhs,
    const gpu::Mailbox& rhs) const {
  return lhs.mailbox() < rhs;
}
bool SkiaOutputDeviceBufferQueue::OverlayDataComparator::operator()(
    const gpu::Mailbox& lhs,
    const OverlayData& rhs) const {
  return lhs < rhs.mailbox();
}

}  // namespace viz
