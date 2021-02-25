// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/alias.h"
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
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace {
base::TimeTicks g_last_reshape_failure = base::TimeTicks();

NOINLINE void CheckForLoopFailuresBufferQueue() {
  const auto threshold = base::TimeDelta::FromSeconds(1);
  auto now = base::TimeTicks::Now();
  if (!g_last_reshape_failure.is_null() &&
      now - g_last_reshape_failure < threshold) {
    CHECK(false);
  }
  g_last_reshape_failure = now;
}

}  // namespace

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

  void OnContextLost() { representation_->OnContextLost(); }

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
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    bool needs_background_image)
    : SkiaOutputDevice(deps->GetSharedContextState()->gr_context(),
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      presenter_(std::move(presenter)),
      context_state_(deps->GetSharedContextState()),
      representation_factory_(representation_factory),
      needs_background_image_(needs_background_image) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.preserve_buffer_content = true;
  capabilities_.only_invalidates_damage_rect = false;
  capabilities_.number_of_buffers = 3;
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kHardware;

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
  // TODO(vasilyt): We should not need this when we stop using
  // SharedImageBackingGLImage.
  if (context_state_->context_lost()) {
    for (auto& overlay : overlays_) {
      overlay.OnContextLost();
    }

    for (auto& image : images_) {
      image->OnContextLost();
    }
  }

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
    DCHECK_EQ(displayed_image_->GetPresentCount() > 1,
              displayed_image_ == image);
    displayed_image_->EndPresent();
    if (!displayed_image_->GetPresentCount()) {
      available_images_.push_back(displayed_image_);
      // Call BeginWriteSkia() for the next frame here to avoid some expensive
      // operations on the critical code path.
      if (!available_images_.front()->sk_surface() &&
          context_state_->MakeCurrent(nullptr)) {
        // BeginWriteSkia() may alter GL's state.
        context_state_->set_need_context_state_reset(true);
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
  if (background_image_ && !background_image_is_scheduled_) {
    background_image_->BeginPresent();
    presenter_->ScheduleBackground(background_image_.get());
    background_image_is_scheduled_ = true;
  }

  if (plane) {
    // If the current_image_ is nullptr, it means there is no change on the
    // primary plane. So we just need to schedule the last submitted image.
    auto* image = current_image_ ? current_image_ : submitted_image_;
    // |image| can be null if there was a fullscreen overlay last frame (e.g.
    // no primary plane). If the fullscreen quad suddenly fails the fullscreen
    // overlay check this frame (e.g. TestPageFlip failing) and then gets
    // promoted via a different strategy like single-on-top, the quad's damage
    // is still removed from the primary plane's damage. With no damage, we
    // never invoke |BeginPaint| which initializes a new image. Since there
    // still really isn't any primary plane content, it's fine to early-exit.
    if (!image && primary_plane_waiting_on_paint_)
      return;
    DCHECK(image);

    image->BeginPresent();
    presenter_->SchedulePrimaryPlane(plane.value(), image,
                                     image == submitted_image_);
  } else {
    primary_plane_waiting_on_paint_ =  true;
    current_frame_has_no_primary_plane_ = true;
    // Even if there is no primary plane, |current_image_| may be non-null if
    // an overlay just transitioned from an underlay strategy to a fullscreen
    // strategy (e.g. a the media controls disappearing on a fullscreen video).
    // In this case, there is still damage which triggers a render pass, but
    // since we promote via fullscreen, we remove the primary plane in the end.
    // We need to recycle |current_image_| to avoid a use-after-free error.
    if (current_image_) {
      available_images_.push_back(current_image_);
      current_image_ = nullptr;
    }
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

    // Fuchsia does not provide a GLImage overlay.
#if defined(OS_FUCHSIA)
    const bool needs_gl_image = false;
#else
    const bool needs_gl_image = true;
#endif  // defined(OS_FUCHSIA)

    // TODO(penghuang): do not depend on GLImage.
    auto shared_image_access =
        shared_image->BeginScopedReadAccess(needs_gl_image);
    if (!shared_image_access) {
      LOG(ERROR) << "Could not access SharedImage for read.";
      continue;
    }

    // TODO(penghuang): do not depend on GLImage.
    DLOG_IF(FATAL, needs_gl_image && !shared_image_access->gl_image())
        << "Cannot get GLImage.";

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

void SkiaOutputDeviceBufferQueue::Submit(bool sync_cpu,
                                         base::OnceClosure callback) {
  // The current image may be missing, for example during WebXR presentation.
  // The SkSurface may also be missing due to a rare edge case (seen at ~1CPM
  // on CrOS)- if we end up skipping the swap for a frame and don't have
  // damage in the next frame (e.g.fullscreen overlay),
  // |current_image_->BeginWriteSkia| won't get called before |Submit|. In
  // this case, we shouldn't call |PreGrContextSubmit| since there's no active
  // surface to flush.
  if (current_image_ && current_image_->sk_surface())
    current_image_->PreGrContextSubmit();

  SkiaOutputDevice::Submit(sync_cpu, std::move(callback));
}

void SkiaOutputDeviceBufferQueue::SwapBuffers(BufferPresentedCallback feedback,
                                              OutputSurfaceFrame frame) {
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
          base::Unretained(this), GetSwapBuffersSize(), std::move(frame),
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
    OutputSurfaceFrame frame) {
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
          base::Unretained(this), GetSwapBuffersSize(), std::move(frame),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->PostSubBuffer(rect, swap_completion_callbacks_.back()->callback(),
                            std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
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
          base::Unretained(this), GetSwapBuffersSize(), std::move(frame),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->CommitOverlayPlanes(swap_completion_callbacks_.back()->callback(),
                                  std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers(
    const gfx::Size& size,
    OutputSurfaceFrame frame,
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

  // Code below can destroy last representation of the overlay shared image. On
  // MacOS it needs context to be current.
#if defined(OS_APPLE)
  // TODO(vasilyt): We shouldn't need this after we stop using
  // SharedImageBackingGLImage as backing.
  if (!context_state_->MakeCurrent(nullptr)) {
    for (auto& overlay : overlays_) {
      overlay.OnContextLost();
    }
  }
#endif

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

  bool should_reallocate =
      result.swap_result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;

  DCHECK(!result.gpu_fence);
  const auto& mailbox =
      image ? image->skia_representation()->mailbox() : gpu::Mailbox();
  FinishSwapBuffers(std::move(result), size, std::move(frame),
                    /*damage_area=*/base::nullopt, std::move(released_overlays),
                    mailbox);
  PageFlipComplete(image.get());

  if (should_reallocate)
    RecreateImages();
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
    LOG(ERROR) << "Failed to resize.";
    CheckForLoopFailuresBufferQueue();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }

  color_space_ = color_space;
  image_size_ = size;
  overlay_transform_ = transform;

  if (needs_background_image_ && !background_image_) {
    background_image_ =
        presenter_->AllocateBackgroundImage(color_space_, gfx::Size(4, 4));
    background_image_is_scheduled_ = false;
  }

  bool success = RecreateImages();
  if (!success) {
    CheckForLoopFailuresBufferQueue();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
  }
  return success;
}

bool SkiaOutputDeviceBufferQueue::RecreateImages() {
  FreeAllSurfaces();
  images_ = presenter_->AllocateImages(color_space_, image_size_,
                                       capabilities_.number_of_buffers);
  for (auto& image : images_) {
    available_images_.push_back(image.get());
  }

  return !images_.empty();
}

SkSurface* SkiaOutputDeviceBufferQueue::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  primary_plane_waiting_on_paint_ = false;
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
