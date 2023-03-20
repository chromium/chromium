// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

namespace {
base::TimeTicks g_last_reshape_failure = base::TimeTicks();

NOINLINE void CheckForLoopFailuresBufferQueue() {
  const auto threshold = base::Seconds(1);
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

  OverlayData(std::unique_ptr<gpu::OverlayImageRepresentation> representation,
              std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess>
                  scoped_read_access,
              bool is_root_render_pass)
      : representation_(std::move(representation)),
        scoped_read_access_(std::move(scoped_read_access)),
        ref_(1),
        is_root_render_pass_(is_root_render_pass) {
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
    is_root_render_pass_ = other.is_root_render_pass_;
    return *this;
  }

  bool IsInUseByWindowServer() const {
#if BUILDFLAG(IS_APPLE)
    if (!scoped_read_access_) {
      return false;
    }
    // The root render pass buffers are managed by SkiaRenderer so we don't care
    // if they're in use by the window server.
    if (is_root_render_pass_) {
      return false;
    }
    return scoped_read_access_->IsInUseByWindowServer();
#else
    return false;
#endif
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
  gpu::OverlayImageRepresentation::ScopedReadAccess* scoped_read_access()
      const {
    return scoped_read_access_.get();
  }

  bool IsRootRenderPass() { return is_root_render_pass_; }

 private:
  void Reset() {
    scoped_read_access_.reset();
    representation_.reset();
    ref_ = 0;
  }

  std::unique_ptr<gpu::OverlayImageRepresentation> representation_;
  std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess>
      scoped_read_access_;
  int ref_ = 0;
  bool is_root_render_pass_ = false;
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
      workarounds_(deps->GetGpuDriverBugWorkarounds()),
      context_state_(deps->GetSharedContextState()),
      representation_factory_(representation_factory) {
#if BUILDFLAG(IS_OZONE)
  capabilities_.needs_background_image = ui::OzonePlatform::GetInstance()
                                             ->GetPlatformRuntimeProperties()
                                             .needs_background_image;
  capabilities_.supports_non_backed_solid_color_overlays =
      ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_non_backed_solid_color_buffers;
#elif BUILDFLAG(IS_APPLE)
  capabilities_.supports_non_backed_solid_color_overlays = true;
#endif  // BUILDFLAG(IS_OZONE)

  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.preserve_buffer_content = true;
  capabilities_.only_invalidates_damage_rect = false;
  capabilities_.number_of_buffers = 3;
  capabilities_.supports_gpu_vsync = presenter_->SupportsGpuVSync();

  capabilities_.renderer_allocates_images =
      ::features::ShouldRendererAllocateImages();

#if BUILDFLAG(IS_ANDROID)
  if (::features::IncreaseBufferCountForHighFrameRate()) {
    capabilities_.number_of_buffers = 5;
  }
#endif
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kHardware;

  // Force the number of max pending frames to one when the switch
  // "double-buffer-compositing" is passed.
  // This will keep compositing in double buffered mode assuming |buffer_queue|
  // allocates at most one additional buffer.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDoubleBufferCompositing))
    capabilities_.number_of_buffers = 2;
  capabilities_.pending_swap_params.max_pending_swaps =
      capabilities_.number_of_buffers - 1;
#if BUILDFLAG(IS_ANDROID)
  if (::features::IncreaseBufferCountForHighFrameRate() &&
      capabilities_.number_of_buffers == 5) {
    capabilities_.pending_swap_params.max_pending_swaps = 2;
    capabilities_.pending_swap_params.max_pending_swaps_90hz = 3;
    capabilities_.pending_swap_params.max_pending_swaps_120hz = 4;
  }
#endif
  DCHECK_LT(capabilities_.pending_swap_params.max_pending_swaps,
            capabilities_.number_of_buffers);
  DCHECK_LT(
      capabilities_.pending_swap_params.max_pending_swaps_90hz.value_or(0),
      capabilities_.number_of_buffers);
  DCHECK_LT(
      capabilities_.pending_swap_params.max_pending_swaps_120hz.value_or(0),
      capabilities_.number_of_buffers);

  presenter_->InitializeCapabilities(&capabilities_);

  if (capabilities_.supports_post_sub_buffer)
    capabilities_.supports_target_damage = true;
}

SkiaOutputDeviceBufferQueue::~SkiaOutputDeviceBufferQueue() {
  // TODO(vasilyt): We should not need this when we stop using
  // GLImageBacking.
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
  DCHECK(!capabilities_.renderer_allocates_images);
  CHECK(!available_images_.empty());
  auto* image = available_images_.front();
  available_images_.pop_front();
  return image;
}

void SkiaOutputDeviceBufferQueue::PageFlipComplete(
    OutputPresenter::Image* image,
    gfx::GpuFenceHandle release_fence) {
  if (displayed_image_) {
    DCHECK(!capabilities_.renderer_allocates_images);
    DCHECK_EQ(displayed_image_->skia_representation()->size(), image_size_);
    DCHECK_EQ(displayed_image_->GetPresentCount() > 1,
              displayed_image_ == image);
    // MakeCurrent is necessary for inserting release fences and for
    // BeginWriteSkia below.
    context_state_->MakeCurrent(/*surface=*/nullptr);
    displayed_image_->EndPresent(std::move(release_fence));
    if (!displayed_image_->GetPresentCount()) {
      available_images_.push_back(displayed_image_);
      // Call BeginWriteSkia() for the next frame here to avoid some expensive
      // operations on the critical code path. Do this only if we wrote to an
      // image this frame (if we did not, assume we will not for the next
      // frame).
      if (!available_images_.front()->sk_surface() && image) {
        // BeginWriteSkia() may alter GL's state.
        context_state_->set_need_context_state_reset(true);
        available_images_.front()->BeginWriteSkia(sample_count_);
      }
    }
  }

  displayed_image_ = image;
  swap_completion_callbacks_.pop_front();

  // If there is no displayed image, then purge one available image.
  if (base::FeatureList::IsEnabled(features::kBufferQueueImageSetPurgeable)) {
    if (!displayed_image_) {
      for (auto* image_to_discard : available_images_) {
        if (image_to_discard->SetPurgeable())
          break;
      }
    }
  }
}

void SkiaOutputDeviceBufferQueue::FreeAllSurfaces() {
  images_.clear();
  current_image_ = nullptr;
  submitted_image_ = nullptr;
  displayed_image_ = nullptr;
  available_images_.clear();
  primary_plane_waiting_on_paint_ = true;
}

bool SkiaOutputDeviceBufferQueue::IsPrimaryPlaneOverlay() const {
  return true;
}

void SkiaOutputDeviceBufferQueue::SchedulePrimaryPlane(
    const absl::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>&
        plane) {
  if (plane) {
    DCHECK(!capabilities_.renderer_allocates_images);
    // If the current_image_ is nullptr, it means there is no change on the
    // primary plane. So we just need to schedule the last submitted image.
    auto* image =
        current_image_ ? current_image_.get() : submitted_image_.get();
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

SkiaOutputDeviceBufferQueue::OverlayData*
SkiaOutputDeviceBufferQueue::GetOrCreateOverlayData(const gpu::Mailbox& mailbox,
                                                    bool is_root_render_pass,
                                                    bool* is_existing) {
  if (is_existing)
    *is_existing = false;

  if (!mailbox.IsSharedImage())
    return nullptr;

  auto it = overlays_.find(mailbox);
  if (it != overlays_.end()) {
    // If the overlay is in |overlays_|, we will reuse it, and a ref will be
    // added to keep it alive. This ref will be removed, when the overlay is
    // replaced by a new frame.
    it->Ref();
    if (is_existing)
      *is_existing = true;
    return &*it;
  }

  auto shared_image = representation_factory_->ProduceOverlay(mailbox);
  // When display is re-opened, the first few frames might not have video
  // resource ready. Possible investigation crbug.com/1023971.
  if (!shared_image) {
    LOG(ERROR) << "Invalid mailbox.";
    return nullptr;
  }

  auto shared_image_access = shared_image->BeginScopedReadAccess();
  if (!shared_image_access) {
    LOG(ERROR) << "Could not access SharedImage for read.";
    return nullptr;
  }

  bool result;
  std::tie(it, result) =
      overlays_.emplace(std::move(shared_image), std::move(shared_image_access),
                        is_root_render_pass);
  DCHECK(result);
  DCHECK(it->unique());

  // Add an extra ref to keep it alive. This extra ref will be removed when
  // the backing is not used by system compositor anymore.
  it->Ref();
  return &*it;
}

void SkiaOutputDeviceBufferQueue::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  DCHECK(pending_overlay_mailboxes_.empty());

  // The fence that will be created for current ScheduleOverlays. This fence is
  // required and passed with overlay data iff DelegatedCompositing is enabled
  // and the overlay's shared image backing is created for raster op. Given
  // rasterization tasks create fences when gpu operations are issued, we end up
  // having multiple number of fences, which creation is costly. Instead, a
  // single fence is created during overlays' scheduling, which is dupped and
  // inserted into each OverlayPlaneData if the underlying shared image was
  // created for rasterization.
  //
  // TODO(msisov): find a better place for this fence.
  std::unique_ptr<gfx::GpuFence> current_frame_fence;

  for (const auto& overlay : overlays) {
    auto mailbox = overlay.mailbox;
#if BUILDFLAG(IS_OZONE)
    if (overlay.is_solid_color) {
      DCHECK(overlay.color.has_value());
      DCHECK(capabilities_.supports_non_backed_solid_color_overlays);
      presenter_->ScheduleOverlayPlane(overlay, nullptr, nullptr);
      continue;
    }
#endif

    OutputPresenter::ScopedOverlayAccess* access = nullptr;
    bool overlay_has_been_submitted;
    auto* overlay_data = GetOrCreateOverlayData(
        mailbox, overlay.is_root_render_pass, &overlay_has_been_submitted);
    if (overlay_data) {
      access = overlay_data->scoped_read_access();
      pending_overlay_mailboxes_.emplace_back(mailbox);
    }

    std::unique_ptr<gfx::GpuFence> acquire_fence;
    if (context_state_->GrContextIsGL() && access &&
        !overlay_has_been_submitted &&
        (access->representation()->usage() &
         gpu::SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING) &&
        gl::GLFence::IsGpuFenceSupported()) {
      DCHECK(features::IsDelegatedCompositingEnabled());
      // Create a single fence that will be duplicated and inserted into each
      // overlay plane data. This avoids unnecessary cost as creating multiple
      // number of fences at the end of each raster task at the ShareImage
      // level is costly. Thus, at this point, the gpu tasks have been
      // dispatched and it's safe to create just a single fence.
      if (!current_frame_fence)
        current_frame_fence = gl::GLFence::CreateForGpuFence()->GetGpuFence();

      // Dup the fence - it must be inserted into each shared image before
      // ScopedReadAccess is created.
      acquire_fence = std::make_unique<gfx::GpuFence>(
          current_frame_fence->GetGpuFenceHandle().Clone());
    }

    presenter_->ScheduleOverlayPlane(overlay, access, std::move(acquire_fence));
  }
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

void SkiaOutputDeviceBufferQueue::Present(
    const absl::optional<gfx::Rect>& update_rect,
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

#if BUILDFLAG(IS_APPLE)
  presenter_->SetCALayerErrorCode(frame.ca_layer_error_code);
#endif

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  auto data = frame.data;
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), GetSwapBuffersSize(), std::move(frame),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->Present(swap_completion_callbacks_.back()->callback(),
                      std::move(feedback), data);
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

  bool need_gl_context = false;
#if BUILDFLAG(IS_APPLE)
  // TODO(vasilyt): We shouldn't need this after we stop using
  // GLImageBacking as backing.
  need_gl_context = true;
#elif BUILDFLAG(IS_OZONE)
  // GL textures are cached in OzoneImageBacking with this workaround and when
  // overlay representations are destroyed, backing may get destroyed leading to
  // GL texture destruction. This destruction needs GL context current.
  if (workarounds_.cache_texture_in_ozone_backing) {
    need_gl_context = true;
  }
#endif

  // Code below can destroy last representation of the overlay shared image. On
  // MacOS and Ozone platforms it needs context to be current.
  if (need_gl_context) {
    if (!context_state_->MakeCurrent(nullptr)) {
      for (auto& overlay : overlays_) {
        overlay.OnContextLost();
      }
    }
  }

  [[maybe_unused]] std::vector<gpu::Mailbox> released_overlays;
  // Go through backings of all overlays, and release overlay backings which are
  // not used.
  base::EraseIf(overlays_, [&result, &released_overlays](auto& overlay) {
    if (!overlay.unique()) {
      return false;
    }

    if (overlay.IsInUseByWindowServer()) {
      return false;
    }

    // Right now, only macOS and LaCros needs to return maliboxes of released
    // overlays, so SkiaRenderer can unlock resources for them.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
    // The root render pass buffers are managed by SkiaRenderer so we don't need
    // to explicitly return them via callback.
    if (!overlay.IsRootRenderPass()) {
      released_overlays.push_back(overlay.mailbox());
    }
#else
    (void)released_overlays;
#endif
    // Setting fences on overlays every frame can be very costly for delegated
    // compositing where we have an overlay for each visible quad. So we only
    // set the release fence here iff this is the last 'Unref' call.
    if (!result.release_fence.is_null()) {
      overlay.scoped_read_access()->SetReleaseFence(
          result.release_fence.Clone());
    }
    overlay.Unref();
    return true;
  });

  bool should_reallocate =
      result.swap_result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;

  const auto& mailbox =
      image ? image->skia_representation()->mailbox() : gpu::Mailbox();
  auto release_fence = result.release_fence.Clone();
  FinishSwapBuffers(std::move(result), size, std::move(frame),
                    /*damage_area=*/absl::nullopt, std::move(released_overlays),
                    mailbox);
  PageFlipComplete(image.get(), std::move(release_fence));

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

bool SkiaOutputDeviceBufferQueue::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  DCHECK(pending_overlay_mailboxes_.empty());
  if (!presenter_->Reshape(characterization, color_space, device_scale_factor,
                           transform)) {
    LOG(ERROR) << "Failed to resize.";
    CheckForLoopFailuresBufferQueue();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }

  overlay_transform_ = transform;
  gfx::Size size = gfx::SkISizeToSize(characterization.dimensions());
  if (color_space_ == color_space && image_size_ == size)
    return true;
  color_space_ = color_space;
  image_size_ = size;
  sample_count_ = characterization.sampleCount();

  bool success = RecreateImages();
  if (!success) {
    CheckForLoopFailuresBufferQueue();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
  }
  return success;
}

void SkiaOutputDeviceBufferQueue::SetViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

bool SkiaOutputDeviceBufferQueue::RecreateImages() {
  if (capabilities_.renderer_allocates_images) {
    return true;
  }
  FreeAllSurfaces();
  size_t number_to_allocate =
      capabilities_.supports_dynamic_frame_buffer_allocation
          ? number_of_images_to_allocate_
          : capabilities_.number_of_buffers;
  if (!number_to_allocate)
    return true;

  images_ =
      presenter_->AllocateImages(color_space_, image_size_, number_to_allocate);
  for (auto& image : images_) {
    available_images_.push_back(image.get());
  }

  DCHECK(images_.empty() || images_.size() == number_to_allocate);
  return !images_.empty();
}

SkSurface* SkiaOutputDeviceBufferQueue::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(!capabilities_.renderer_allocates_images);
  primary_plane_waiting_on_paint_ = false;

  if (!current_image_) {
    current_image_ = GetNextImage();
  }

  if (!current_image_->sk_surface())
    current_image_->BeginWriteSkia(sample_count_);
  *end_semaphores = current_image_->TakeEndWriteSkiaSemaphores();
  return current_image_->sk_surface();
}

void SkiaOutputDeviceBufferQueue::EndPaint() {
  DCHECK(!capabilities_.renderer_allocates_images);
  DCHECK(current_image_);
  current_image_->EndWriteSkia();
}

bool SkiaOutputDeviceBufferQueue::EnsureMinNumberOfBuffers(size_t n) {
  DCHECK(!capabilities_.renderer_allocates_images);
  DCHECK(capabilities_.supports_dynamic_frame_buffer_allocation);
  DCHECK_GT(n, 0u);
  DCHECK_LE(n, static_cast<size_t>(capabilities_.number_of_buffers));

  if (number_of_images_to_allocate_ >= n)
    return true;
  number_of_images_to_allocate_ = n;
  if (image_size_.IsEmpty())
    return true;
  return RecreateImages();
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

void SkiaOutputDeviceBufferQueue::SetGpuVSyncEnabled(bool enabled) {
  presenter_->SetGpuVSyncEnabled(enabled);
}

void SkiaOutputDeviceBufferQueue::SetVSyncDisplayID(int64_t display_id) {
  presenter_->SetVSyncDisplayID(display_id);
}

}  // namespace viz
