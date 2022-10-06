// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_gl.h"

#include <tuple>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/alias.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#include "ui/gl/dc_renderer_layer_params.h"
#endif

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
class SkiaOutputDeviceGL::OverlayData {
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

  gpu::OverlayImageRepresentation::ScopedReadAccess* BeginOverlayAccess() {
    DCHECK(representation_);
    access_ = representation_->BeginScopedReadAccess(/*needs_gl_image=*/true);
    DCHECK(access_);
    return access_.get();
  }

  void EndOverlayAccess() { access_.reset(); }

 private:
  std::unique_ptr<gpu::OverlayImageRepresentation> representation_;
  std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess> access_;
};

SkiaOutputDeviceGL::SkiaOutputDeviceGL(
    gpu::MailboxManager* mailbox_manager,
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      mailbox_manager_(mailbox_manager),
      shared_image_representation_factory_(shared_image_representation_factory),
      context_state_(context_state),
      gl_surface_(std::move(gl_surface)),
      supports_async_swap_(gl_surface_->SupportsAsyncSwap()) {
  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gl_surface_->GetOrigin();
  capabilities_.supports_post_sub_buffer = gl_surface_->SupportsPostSubBuffer();
#if BUILDFLAG(IS_WIN)
  if (gl_surface_->SupportsDCLayers()) {
    // DWM handles preserving the contents of the backbuffer in Present1, so we
    // don't need to have SkiaOutputSurface handle it.
    capabilities_.preserve_buffer_content = false;
    capabilities_.number_of_buffers =
        gl::DirectCompositionRootSurfaceBufferCount();
    capabilities_.supports_delegated_ink = gl_surface_->SupportsDelegatedInk();
  }
#endif  // BUILDFLAG(IS_WIN)
  if (feature_info->workarounds()
          .disable_post_sub_buffers_for_onscreen_surfaces) {
    capabilities_.supports_post_sub_buffer = false;
  }
  if (feature_info->workarounds().supports_two_yuv_hardware_overlays) {
    capabilities_.supports_two_yuv_hardware_overlays = true;
  }
  capabilities_.pending_swap_params.max_pending_swaps =
      gl_surface_->GetBufferCount() - 1;
  capabilities_.supports_commit_overlay_planes =
      gl_surface_->SupportsCommitOverlayPlanes();
  capabilities_.supports_gpu_vsync = gl_surface_->SupportsGpuVSync();
  capabilities_.supports_dc_layers = gl_surface_->SupportsDCLayers();
#if BUILDFLAG(IS_ANDROID)
  // TODO(weiliangc): This capability is used to check whether we should do
  // overlay. Since currently none of the other overlay system is implemented,
  // only update this for Android.
  // This output device is never offscreen.
  capabilities_.supports_surfaceless = gl_surface_->IsSurfaceless();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If Chrome OS is run on Linux for development purposes, we need to
  // advertise a hardware orientation mode since Ash manages a separate device
  // rotation independent of the host's native windowing system.
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kHardware;
#endif  // IS_CHROMEOS_ASH

  DCHECK(context_state_);
  DCHECK(gl_surface_);

  if (gl_surface_->SupportsSwapTimestamps()) {
    gl_surface_->SetEnableSwapTimestamps();

    // Changes to swap timestamp queries are only picked up when making current.
    context_state_->ReleaseCurrent(nullptr);
    context_state_->MakeCurrent(gl_surface_.get());
  }

  DCHECK(context_state_->gr_context());
  DCHECK(context_state_->context());

  GrDirectContext* gr_context = context_state_->gr_context();
  gl::CurrentGL* current_gl = context_state_->context()->GetCurrentGL();

  // Get alpha bits from the default frame buffer.
  int alpha_bits = 0;
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  gr_context->resetContext(kRenderTarget_GrGLBackendState);
  const auto* version = current_gl->Version;
  if (version->is_desktop_core_profile) {
    glGetFramebufferAttachmentParameterivEXT(
        GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
        &alpha_bits);
  } else {
    glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
  }
  CHECK_GL_ERROR();

  auto color_type = kRGBA_8888_SkColorType;

  if (alpha_bits == 0) {
    color_type = gl_surface_->GetFormat().GetBufferSize() == 16
                     ? kRGB_565_SkColorType
                     : kRGB_888x_SkColorType;
    // Skia disables RGBx on some GPUs, fallback to RGBA if it's the
    // case. This doesn't change framebuffer itself, as we already allocated it,
    // but will change any temporary buffer Skia needs to allocate.
    if (!context_state_->gr_context()
             ->defaultBackendFormat(color_type, GrRenderable::kYes)
             .isValid()) {
      color_type = kRGBA_8888_SkColorType;
    }
  }
  // SRGB
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      color_type;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      color_type;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      color_type;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      color_type;
  // HDR10
  capabilities_
      .sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_1010102)] =
      kRGBA_1010102_SkColorType;
  // scRGB linear
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_F16)] =
      kRGBA_F16_SkColorType;
}

SkiaOutputDeviceGL::~SkiaOutputDeviceGL() {
  // gl_surface_ will be destructed soon.
  memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
}

bool SkiaOutputDeviceGL::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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

void SkiaOutputDeviceGL::SwapBuffers(BufferPresentedCallback feedback,
                                     OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  auto data = std::move(frame.data);
  if (supports_async_swap_) {
    auto callback = base::BindOnce(
        &SkiaOutputDeviceGL::DoFinishSwapBuffersAsync,
        weak_ptr_factory_.GetWeakPtr(), surface_size, std::move(frame));
    gl_surface_->SwapBuffersAsync(std::move(callback), std::move(feedback),
                                  std::move(data));
  } else {
    gfx::SwapResult result =
        gl_surface_->SwapBuffers(std::move(feedback), std::move(data));
    DoFinishSwapBuffers(surface_size, std::move(frame),
                        gfx::SwapCompletionResult(result));
  }
}

void SkiaOutputDeviceGL::PostSubBuffer(const gfx::Rect& rect,
                                       BufferPresentedCallback feedback,
                                       OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  auto data = std::move(frame.data);
  if (supports_async_swap_) {
    auto callback = base::BindOnce(
        &SkiaOutputDeviceGL::DoFinishSwapBuffersAsync,
        weak_ptr_factory_.GetWeakPtr(), surface_size, std::move(frame));
    gl_surface_->PostSubBufferAsync(rect.x(), rect.y(), rect.width(),
                                    rect.height(), std::move(callback),
                                    std::move(feedback), std::move(data));
  } else {
    gfx::SwapResult result = gl_surface_->PostSubBuffer(
        rect.x(), rect.y(), rect.width(), rect.height(), std::move(feedback),
        std::move(data));
    DoFinishSwapBuffers(surface_size, std::move(frame),
                        gfx::SwapCompletionResult(result));
  }
}

void SkiaOutputDeviceGL::CommitOverlayPlanes(BufferPresentedCallback feedback,
                                             OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  auto data = std::move(frame.data);
  if (supports_async_swap_) {
    auto callback = base::BindOnce(
        &SkiaOutputDeviceGL::DoFinishSwapBuffersAsync,
        weak_ptr_factory_.GetWeakPtr(), surface_size, std::move(frame));
    gl_surface_->CommitOverlayPlanesAsync(std::move(callback),
                                          std::move(feedback), std::move(data));
  } else {
    gfx::SwapResult result =
        gl_surface_->CommitOverlayPlanes(std::move(feedback), std::move(data));
    DoFinishSwapBuffers(surface_size, std::move(frame),
                        gfx::SwapCompletionResult(result));
  }
}

void SkiaOutputDeviceGL::DoFinishSwapBuffersAsync(
    const gfx::Size& size,
    OutputSurfaceFrame frame,
    gfx::SwapCompletionResult result) {
  DCHECK(result.release_fence.is_null());
  FinishSwapBuffers(std::move(result), size, std::move(frame));
}

void SkiaOutputDeviceGL::DoFinishSwapBuffers(const gfx::Size& size,
                                             OutputSurfaceFrame frame,
                                             gfx::SwapCompletionResult result) {
  DCHECK(result.release_fence.is_null());

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

  FinishSwapBuffers(std::move(result), size, std::move(frame));
}

bool SkiaOutputDeviceGL::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  return gl_surface_->SetDrawRectangle(draw_rectangle);
}

void SkiaOutputDeviceGL::SetGpuVSyncEnabled(bool enabled) {
  gl_surface_->SetGpuVSyncEnabled(enabled);
}

void SkiaOutputDeviceGL::SetEnableDCLayers(bool enable) {
  gl_surface_->SetEnableDCLayers(enable);
}

void SkiaOutputDeviceGL::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
#if BUILDFLAG(IS_WIN)
  for (auto& dc_layer : overlays) {
    auto params = std::make_unique<ui::DCRendererLayerParams>();
    // Get GLImages for DC layer textures.
    bool success = true;
    for (size_t i = 0; i < DCLayerOverlay::kNumResources; ++i) {
      const gpu::Mailbox& mailbox = dc_layer.mailbox[i];
      if (i > 0 && mailbox.IsZero())
        break;

      auto* read_access = BeginOverlayAccess(mailbox);
      if (!read_access) {
        success = false;
        break;
      }

      if (auto dcomp_surface_proxy = read_access->GetDCOMPSurfaceProxy()) {
        params->dcomp_surface_proxy = std::move(dcomp_surface_proxy);
      } else if (auto* image = read_access->gl_image()) {
        image->SetColorSpace(dc_layer.color_space);
        params->images[i] = std::move(image);
      } else {
        success = false;
        break;
      }

      scheduled_overlay_mailboxes_.insert(mailbox);
    }

    if (!success) {
      DLOG(ERROR) << "Failed to get GLImage for DC layer.";
      continue;
    }

    params->z_order = dc_layer.z_order;
    params->content_rect = dc_layer.content_rect;
    params->quad_rect = dc_layer.quad_rect;
    DCHECK(dc_layer.transform.IsFlat());
    params->transform = dc_layer.transform;
    params->clip_rect = dc_layer.clip_rect;
    params->protected_video_type = dc_layer.protected_video_type;
    params->hdr_metadata = dc_layer.hdr_metadata;
    params->is_video_fullscreen_letterboxing =
        dc_layer.is_video_fullscreen_letterboxing;

    // Schedule DC layer overlay to be presented at next SwapBuffers().
    if (!gl_surface_->ScheduleDCLayer(std::move(params)))
      DLOG(ERROR) << "ScheduleDCLayer failed";
  }
#endif  // BUILDFLAG(IS_WIN)
}

void SkiaOutputDeviceGL::EnsureBackbuffer() {
  gl_surface_->SetBackbufferAllocation(true);
}

void SkiaOutputDeviceGL::DiscardBackbuffer() {
  gl_surface_->SetBackbufferAllocation(false);
}

SkSurface* SkiaOutputDeviceGL::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(sk_surface_);
  return sk_surface_.get();
}

void SkiaOutputDeviceGL::EndPaint() {}

gpu::OverlayImageRepresentation::ScopedReadAccess*
SkiaOutputDeviceGL::BeginOverlayAccess(const gpu::Mailbox& mailbox) {
  auto it = overlays_.find(mailbox);
  if (it != overlays_.end())
    return it->second.BeginOverlayAccess();

  auto overlay = shared_image_representation_factory_->ProduceOverlay(mailbox);
  if (!overlay)
    return nullptr;

  std::tie(it, std::ignore) = overlays_.emplace(mailbox, std::move(overlay));
  return it->second.BeginOverlayAccess();
}

}  // namespace viz
