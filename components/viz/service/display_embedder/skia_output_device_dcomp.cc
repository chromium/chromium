// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"

#include <tuple>
#include <utility>

#include "base/containers/cxx20_erase.h"
#include "base/debug/alias.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

namespace viz {

namespace {
base::TimeTicks g_last_reshape_failure = base::TimeTicks();

NOINLINE void CheckForLoopFailures() {
  const auto threshold = base::Seconds(1);
  auto now = base::TimeTicks::Now();
  if (!g_last_reshape_failure.is_null() &&
      now - g_last_reshape_failure < threshold) {
    CHECK(false);
  }
  g_last_reshape_failure = now;
}

}  // namespace

// Holds reference needed to keep overlay textures alive.
// TODO(kylechar): We can probably merge OverlayData in with
// SkiaOutputSurfaceImplOnGpu overlay data.
class SkiaOutputDeviceDComp::OverlayData {
 public:
  explicit OverlayData(
      std::unique_ptr<gpu::OverlayImageRepresentation> representation)
      : representation_(std::move(representation)) {}

  ~OverlayData() = default;
  OverlayData(OverlayData&& other) = default;
  OverlayData& operator=(OverlayData&& other) {
    // `access_` must be overwritten before `representation_`.
    access_ = std::move(other.access_);
    representation_ = std::move(other.representation_);
    return *this;
  }

  absl::optional<gl::DCLayerOverlayImage> BeginOverlayAccess() {
    DCHECK(representation_);
    access_ = representation_->BeginScopedReadAccess();
    DCHECK(access_);
    return access_->GetDCLayerOverlayImage();
  }

  void EndOverlayAccess() { access_.reset(); }

 private:
  std::unique_ptr<gpu::OverlayImageRepresentation> representation_;
  std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess> access_;
};

SkiaOutputDeviceDComp::SkiaOutputDeviceDComp(
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::SharedContextState* context_state,
    gl::GLSurface* gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      shared_image_representation_factory_(shared_image_representation_factory),
      context_state_(context_state) {
  DCHECK(!feature_info->workarounds()
              .disable_post_sub_buffers_for_onscreen_surfaces);
  DCHECK(gl_surface->SupportsDCLayers());
  DCHECK_EQ(gl_surface->GetOrigin(), gfx::SurfaceOrigin::kTopLeft);
  DCHECK(gl_surface->SupportsGpuVSync());
  DCHECK(!gl_surface->SupportsCommitOverlayPlanes());

  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities_.supports_post_sub_buffer = gl_surface->SupportsPostSubBuffer();
  // DWM handles preserving the contents of the backbuffer in Present1, so we
  // don't need to have SkiaOutputSurface handle it.
  capabilities_.preserve_buffer_content = false;
  capabilities_.number_of_buffers =
      gl::DirectCompositionRootSurfaceBufferCount();
  capabilities_.supports_delegated_ink = gl_surface->SupportsDelegatedInk();
  if (feature_info->workarounds().supports_two_yuv_hardware_overlays) {
    capabilities_.supports_two_yuv_hardware_overlays = true;
  }
  capabilities_.pending_swap_params.max_pending_swaps =
      gl_surface->GetBufferCount() - 1;
  capabilities_.supports_commit_overlay_planes = false;
  capabilities_.supports_gpu_vsync = true;
  capabilities_.supports_dc_layers = true;

  DCHECK(context_state_);
  DCHECK(gl_surface);

  if (gl_surface->SupportsSwapTimestamps()) {
    gl_surface->SetEnableSwapTimestamps();

    // Changes to swap timestamp queries are only picked up when making current.
    context_state_->ReleaseCurrent(nullptr);
    context_state_->MakeCurrent(gl_surface);
  }

  DCHECK(context_state_->gr_context());
  DCHECK(context_state_->context());

  // SRGB
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kRGBA_8888_SkColorType;
  // HDR10
  capabilities_
      .sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_1010102)] =
      kRGBA_1010102_SkColorType;
  // scRGB linear
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_F16)] =
      kRGBA_F16_SkColorType;
}

SkiaOutputDeviceDComp::~SkiaOutputDeviceDComp() = default;

void SkiaOutputDeviceDComp::SwapBuffers(BufferPresentedCallback feedback,
                                        OutputSurfaceFrame frame) {
  PostSubBuffer(gfx::Rect(GetRootSurfaceSize()), std::move(feedback),
                std::move(frame));
}

void SkiaOutputDeviceDComp::PostSubBuffer(const gfx::Rect& rect,
                                          BufferPresentedCallback feedback,
                                          OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  DoPresent(rect,
            base::BindOnce(&SkiaOutputDeviceDComp::OnPresentFinished,
                           weak_ptr_factory_.GetWeakPtr(), std::move(frame)),
            std::move(feedback), frame.data);
}

void SkiaOutputDeviceDComp::OnPresentFinished(
    OutputSurfaceFrame frame,
    gfx::SwapCompletionResult result) {
  // Remove entries from |overlays_| for textures that weren't scheduled as an
  // overlay this frame.
  if (!overlays_.empty()) {
    base::EraseIf(overlays_, [this](auto& entry) {
      const gpu::Mailbox& mailbox = entry.first;
      return !scheduled_overlay_mailboxes_.contains(mailbox);
    });
    scheduled_overlay_mailboxes_.clear();
    // End access for the remaining overlays that were scheduled this frame.
    for (auto& kv : overlays_)
      kv.second.EndOverlayAccess();
  }

  FinishSwapBuffers(std::move(result), GetRootSurfaceSize(), std::move(frame));
}

void SkiaOutputDeviceDComp::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  for (auto& dc_layer : overlays) {
    // Only use the first shared image mailbox for accessing as an overlay.
    const gpu::Mailbox& mailbox = dc_layer.mailbox;
    absl::optional<gl::DCLayerOverlayImage> overlay_image =
        BeginOverlayAccess(mailbox);
    if (!overlay_image) {
      DLOG(ERROR) << "Failed to ProduceOverlay or GetDCLayerOverlayImage";
      continue;
    }

    auto params = std::make_unique<gl::DCLayerOverlayParams>();
    params->overlay_image = std::move(overlay_image);
    params->z_order = dc_layer.plane_z_order;

    // SwapChainPresenter uses the size of the overlay's resource in pixels to
    // calculate its swap chain size. `uv_rect` maps the portion of
    // `resource_size_in_pixels` that will be displayed.
    params->content_rect = gfx::ToNearestRect(gfx::ScaleRect(
        dc_layer.uv_rect, dc_layer.resource_size_in_pixels.width(),
        dc_layer.resource_size_in_pixels.height()));

    params->quad_rect = gfx::ToEnclosingRect(dc_layer.display_rect);
    DCHECK(absl::holds_alternative<gfx::Transform>(dc_layer.transform));
    params->transform = absl::get<gfx::Transform>(dc_layer.transform);
    params->clip_rect = dc_layer.clip_rect;
    params->protected_video_type = dc_layer.protected_video_type;
    params->color_space = dc_layer.color_space;
    params->hdr_metadata = dc_layer.hdr_metadata.value_or(gfx::HDRMetadata());
    params->is_video_fullscreen_letterboxing =
        dc_layer.is_video_fullscreen_letterboxing;

    // Schedule DC layer overlay to be presented at next SwapBuffers().
    if (!ScheduleDCLayer(std::move(params))) {
      DLOG(ERROR) << "ScheduleDCLayer failed";
      continue;
    }
    scheduled_overlay_mailboxes_.insert(mailbox);
  }
}

absl::optional<gl::DCLayerOverlayImage>
SkiaOutputDeviceDComp::BeginOverlayAccess(const gpu::Mailbox& mailbox) {
  auto it = overlays_.find(mailbox);
  if (it != overlays_.end())
    return it->second.BeginOverlayAccess();

  auto overlay = shared_image_representation_factory_->ProduceOverlay(mailbox);
  if (!overlay)
    return absl::nullopt;

  std::tie(it, std::ignore) = overlays_.emplace(mailbox, std::move(overlay));
  return it->second.BeginOverlayAccess();
}

SkiaOutputDeviceDCompGLSurface::SkiaOutputDeviceDCompGLSurface(
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDeviceDComp(shared_image_representation_factory,
                            context_state,
                            gl_surface.get(),
                            std::move(feature_info),
                            memory_tracker,
                            std::move(did_swap_buffer_complete_callback)),
      gl_surface_(std::move(gl_surface)) {
  DCHECK(!gl_surface_->SupportsAsyncSwap());
}

SkiaOutputDeviceDCompGLSurface::~SkiaOutputDeviceDCompGLSurface() {
  // gl_surface_ will be destructed soon.
  memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
}

bool SkiaOutputDeviceDCompGLSurface::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  const gfx::Size size = gfx::SkISizeToSize(characterization.dimensions());
  const SkColorType color_type = characterization.colorType();
  const bool has_alpha =
      !SkAlphaTypeIsOpaque(characterization.imageInfo().alphaType());

  if (!gl_surface_->Resize(size, device_scale_factor, color_space, has_alpha)) {
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }
  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

  GrGLFramebufferInfo framebuffer_info = {0};
  DCHECK_EQ(gl_surface_->GetBackingFramebufferObject(), 0u);

  switch (color_type) {
    case kRGBA_8888_SkColorType:
      framebuffer_info.fFormat = GL_RGBA8;
      break;
    case kRGB_888x_SkColorType:
      framebuffer_info.fFormat = GL_RGB8;
      break;
    case kRGB_565_SkColorType:
      framebuffer_info.fFormat = GL_RGB565;
      break;
    case kRGBA_1010102_SkColorType:
      framebuffer_info.fFormat = GL_RGB10_A2_EXT;
      break;
    case kRGBA_F16_SkColorType:
      framebuffer_info.fFormat = GL_RGBA16F;
      break;
    default:
      NOTREACHED() << "color_type: " << color_type;
  }

  GrBackendRenderTarget render_target(size.width(), size.height(),
                                      characterization.sampleCount(),
                                      /*stencilBits=*/0, framebuffer_info);
  auto origin = (gl_surface_->GetOrigin() == gfx::SurfaceOrigin::kTopLeft)
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      context_state_->gr_context(), render_target, origin, color_type,
      characterization.refColorSpace(), &surface_props);
  if (!sk_surface_) {
    LOG(ERROR) << "Couldn't create surface:"
               << "\n  abandoned()="
               << context_state_->gr_context()->abandoned()
               << "\n  color_type=" << color_type
               << "\n  framebuffer_info.fFBOID=" << framebuffer_info.fFBOID
               << "\n  framebuffer_info.fFormat=" << framebuffer_info.fFormat
               << "\n  color_space=" << color_space.ToString()
               << "\n  size=" << size.ToString();
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
  }

  memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
  GLenum format = gpu::gles2::TextureManager::ExtractFormatFromStorageFormat(
      framebuffer_info.fFormat);
  GLenum type = gpu::gles2::TextureManager::ExtractTypeFromStorageFormat(
      framebuffer_info.fFormat);
  uint32_t estimated_size;
  gpu::gles2::GLES2Util::ComputeImageDataSizes(
      size.width(), size.height(), 1 /* depth */, format, type,
      4 /* alignment */, &estimated_size, nullptr, nullptr);
  backbuffer_estimated_size_ = estimated_size * gl_surface_->GetBufferCount();
  memory_type_tracker_->TrackMemAlloc(backbuffer_estimated_size_);

  return !!sk_surface_;
}

bool SkiaOutputDeviceDCompGLSurface::SetDrawRectangle(
    const gfx::Rect& draw_rectangle) {
  return gl_surface_->SetDrawRectangle(draw_rectangle);
}

void SkiaOutputDeviceDCompGLSurface::SetEnableDCLayers(bool enable) {
  gl_surface_->SetEnableDCLayers(enable);
}

void SkiaOutputDeviceDCompGLSurface::SetGpuVSyncEnabled(bool enabled) {
  gl_surface_->SetGpuVSyncEnabled(enabled);
}

SkSurface* SkiaOutputDeviceDCompGLSurface::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(sk_surface_);
  return sk_surface_.get();
}

void SkiaOutputDeviceDCompGLSurface::EndPaint() {}

bool SkiaOutputDeviceDCompGLSurface::ScheduleDCLayer(
    std::unique_ptr<gl::DCLayerOverlayParams> params) {
  return gl_surface_->ScheduleDCLayer(std::move(params));
}

gfx::Size SkiaOutputDeviceDCompGLSurface::GetRootSurfaceSize() const {
  return gl_surface_->GetSize();
}

void SkiaOutputDeviceDCompGLSurface::DoPresent(
    const gfx::Rect& rect,
    gl::GLSurface::SwapCompletionCallback completed_callback,
    BufferPresentedCallback feedback,
    gfx::FrameData data) {
  gfx::SwapResult result =
      gl_surface_->PostSubBuffer(rect.x(), rect.y(), rect.width(),
                                 rect.height(), std::move(feedback), data);

  // Implement "async" swap synchronously.
  std::move(completed_callback).Run(gfx::SwapCompletionResult(result));
}

SkiaOutputDeviceDCompPresenter::SkiaOutputDeviceDCompPresenter(
    gpu::SharedImageFactory* shared_image_factory,
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::Presenter> presenter,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDeviceDComp(shared_image_representation_factory,
                            context_state,
                            presenter.get(),
                            std::move(feature_info),
                            memory_tracker,
                            std::move(did_swap_buffer_complete_callback)),
      presenter_(std::move(presenter)),
      shared_image_factory_(shared_image_factory) {}

SkiaOutputDeviceDCompPresenter::~SkiaOutputDeviceDCompPresenter() {
  DestroyRootSurface();
}

bool SkiaOutputDeviceDCompPresenter::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  if (!characterization.isValid()) {
    DLOG(ERROR) << "Invalid SkSurfaceCharacterization";
    return false;
  }

  if (characterization_ != characterization || color_space_ != color_space ||
      device_scale_factor_ != device_scale_factor || transform_ != transform) {
    characterization_ = characterization;
    color_space_ = color_space;
    device_scale_factor_ = device_scale_factor;
    transform_ = transform;
    DestroyRootSurface();
  }

  // The |SkiaOutputDeviceDCompPresenter| alpha state depends on
  // |characterization_| and |wants_dcomp_surface_|. Since |presenter_| can only
  // be |DCompPresenter| and its |Resize| function ignores the |has_alpha|
  // parameter, we opt to pass an arbitrary value that we expect to be ignored.
  constexpr bool kDCompPresenterResizeHasAlphaIgnore = false;

  // DCompPresenter calls SetWindowPos on resize, so we call it to reflect the
  // newly allocated root surface.
  // Note, we could inline SetWindowPos here, but we need access to the HWND.
  if (!presenter_->Resize(gfx::SkISizeToSize(characterization_.dimensions()),
                          device_scale_factor_, color_space_,
                          kDCompPresenterResizeHasAlphaIgnore)) {
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }

  return true;
}

bool SkiaOutputDeviceDCompPresenter::SetDrawRectangle(
    const gfx::Rect& draw_rectangle) {
  if (update_rect_.has_value()) {
    DLOG(ERROR) << "SetDrawRectangle must be called only once per "
                   "BeginPaint/EndPaint pair";
    return false;
  }

  if (!presenter_->SetDrawRectangle(draw_rectangle)) {
    return false;
  }

  update_rect_ = draw_rectangle;
  return true;
}

void SkiaOutputDeviceDCompPresenter::SetEnableDCLayers(bool enable) {
  if (want_dcomp_surface_ != enable) {
    want_dcomp_surface_ = enable;

    // Changing this value will require a new root SharedImage
    DestroyRootSurface();
  }
}

void SkiaOutputDeviceDCompPresenter::SetGpuVSyncEnabled(bool enabled) {
  presenter_->SetGpuVSyncEnabled(enabled);
}

bool SkiaOutputDeviceDCompPresenter::EnsureRootSurfaceAllocated() {
  DCHECK(characterization_.isValid()) << "Must call Reshape first";

  if (root_surface_mailbox_.IsZero()) {
    ResourceFormat resource_format =
        SkColorTypeToResourceFormat(characterization_.colorType());

    const gfx::Size size = gfx::SkISizeToSize(characterization_.dimensions());

    // If |want_dcomp_surface_|, it means we are layering the root surface with
    // videos that might be underlays. In this case, we want to force
    // transparency so the underlay appears correctly.
    SkAlphaType alpha_type = want_dcomp_surface_
                                 ? kPremul_SkAlphaType
                                 : characterization_.imageInfo().alphaType();

    // TODO(tangm): DComp surfaces do not support RGB10A2 so we must fall back
    // to swap chains. If this happens with video overlays, this can result in
    // the video overlay and its parent surface having unsynchronized updates.
    // We should clean this up by either avoiding HDR or using RGBAF32 surfaces
    // in this case.
    const bool dcomp_unsupported_format =
        resource_format == ResourceFormat::RGBA_1010102;

    uint32_t usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    if (want_dcomp_surface_ && !dcomp_unsupported_format) {
      usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE;
    } else {
      usage |= gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    }

    gpu::Mailbox root_surface_mailbox = gpu::Mailbox::GenerateForSharedImage();
    bool success = shared_image_factory_->CreateSharedImage(
        root_surface_mailbox, SharedImageFormat::SinglePlane(resource_format),
        size, color_space_, kTopLeft_GrSurfaceOrigin, alpha_type,
        gpu::kNullSurfaceHandle, usage);
    if (!success) {
      CheckForLoopFailures();
      // To prevent tail call, so we can see the stack.
      base::debug::Alias(nullptr);
      return false;
    }

    // Store the root surface's mailbox only on success.
    root_surface_mailbox_ = root_surface_mailbox;
  }

  if (!root_surface_skia_representation_) {
    DCHECK(!root_surface_mailbox_.IsZero());

    root_surface_skia_representation_ =
        shared_image_representation_factory_->ProduceSkia(
            root_surface_mailbox_,
            scoped_refptr<gpu::SharedContextState>(context_state_));

    if (!root_surface_skia_representation_) {
      return false;
    }
  }

  return true;
}

void SkiaOutputDeviceDCompPresenter::DestroyRootSurface() {
  root_surface_write_access_.reset();
  root_surface_skia_representation_.reset();

  if (!root_surface_mailbox_.IsZero()) {
    shared_image_factory_->DestroySharedImage(root_surface_mailbox_);
    root_surface_mailbox_.SetZero();
  }
}

SkSurface* SkiaOutputDeviceDCompPresenter::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!EnsureRootSurfaceAllocated()) {
    DLOG(ERROR) << "Could not create root SharedImage";
    return nullptr;
  }

  DCHECK(root_surface_skia_representation_);
  DCHECK(update_rect_.has_value());

  std::vector<GrBackendSemaphore> begin_semaphores;
  root_surface_write_access_ =
      root_surface_skia_representation_->BeginScopedWriteAccess(
          characterization_.sampleCount(), characterization_.surfaceProps(),
          update_rect_.value(), &begin_semaphores, end_semaphores,
          gpu::SkiaImageRepresentation::AllowUnclearedAccess::kYes, true);
  update_rect_.reset();
  if (!root_surface_write_access_) {
    return nullptr;
  }

  // We don't expect any semaphores on a Windows, non-Vulkan backend.
  DCHECK(begin_semaphores.empty());
  DCHECK(end_semaphores->empty());

  return root_surface_write_access_->surface();
}

void SkiaOutputDeviceDCompPresenter::Submit(bool sync_cpu,
                                            base::OnceClosure callback) {
  if (root_surface_write_access_) {
    // On Windows, we expect `end_state` to be null, since DX11 doesn't use
    // resource states/barriers.
    auto end_state = root_surface_write_access_->TakeEndState();
    DCHECK_EQ(nullptr, end_state);
  }

  SkiaOutputDevice::Submit(sync_cpu, std::move(callback));
}

void SkiaOutputDeviceDCompPresenter::EndPaint() {
  DCHECK(root_surface_skia_representation_);
  DCHECK(root_surface_write_access_);

  // Assume the caller has drawn to everything since the first update rect is
  // required to cover the whole surface.
  root_surface_skia_representation_->SetCleared();

  root_surface_write_access_.reset();
}

bool SkiaOutputDeviceDCompPresenter::IsRootSurfaceAllocatedForTesting() const {
  return !root_surface_mailbox_.IsZero();
}

bool SkiaOutputDeviceDCompPresenter::ScheduleDCLayer(
    std::unique_ptr<gl::DCLayerOverlayParams> params) {
  return presenter_->ScheduleDCLayer(std::move(params));
}

gfx::Size SkiaOutputDeviceDCompPresenter::GetRootSurfaceSize() const {
  return presenter_->GetSize();
}

void SkiaOutputDeviceDCompPresenter::DoPresent(
    const gfx::Rect& rect,
    gl::GLSurface::SwapCompletionCallback completion_callback,
    BufferPresentedCallback feedback,
    gfx::FrameData data) {
  if (!ScheduleRootSurfaceAsOverlay()) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    // Notify the caller, the buffer is never presented on a screen.
    std::move(feedback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  // The |rect| is ignored because SetDrawRectangle specified the area to be
  // swapped.
  presenter_->Present(std::move(completion_callback), std::move(feedback),
                      data);
}

bool SkiaOutputDeviceDCompPresenter::ScheduleRootSurfaceAsOverlay() {
  auto overlay = shared_image_representation_factory_->ProduceOverlay(
      root_surface_mailbox_);
  if (!overlay) {
    return false;
  }

  auto read_access = overlay->BeginScopedReadAccess();
  if (!read_access) {
    return false;
  }

  auto params = std::make_unique<gl::DCLayerOverlayParams>();
  params->z_order = 0;
  params->quad_rect = gfx::Rect(GetRootSurfaceSize());
  params->content_rect = params->quad_rect;
  params->overlay_image = read_access->GetDCLayerOverlayImage();
  ScheduleDCLayer(std::move(params));

  return true;
}

}  // namespace viz
