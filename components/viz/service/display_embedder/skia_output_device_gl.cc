// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_gl.h"

#include <tuple>
#include <utility>

#include "base/debug/alias.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
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

SkiaOutputDeviceGL::SkiaOutputDeviceGL(
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      context_state_(context_state),
      gl_surface_(std::move(gl_surface)),
      supports_async_swap_(gl_surface_->SupportsAsyncSwap()) {
  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gl_surface_->GetOrigin();
  capabilities_.supports_post_sub_buffer = gl_surface_->SupportsPostSubBuffer();
  if (feature_info->workarounds()
          .disable_post_sub_buffers_for_onscreen_surfaces) {
    capabilities_.supports_post_sub_buffer = false;
  }
  if (feature_info->workarounds().supports_two_yuv_hardware_overlays) {
    capabilities_.supports_two_yuv_hardware_overlays = true;
  }
  capabilities_.pending_swap_params.max_pending_swaps =
      gl_surface_->GetBufferCount() - 1;
  capabilities_.supports_gpu_vsync = gl_surface_->SupportsGpuVSync();
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
  const auto* version = current_gl->Version.get();
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

  auto* gr_context = context_state_->gr_context();

  GrBackendFormat backend_format =
      gr_context->defaultBackendFormat(color_type, GrRenderable::kYes);
  DCHECK(backend_format.isValid()) << "color_type: " << color_type;
  framebuffer_info.fFormat = backend_format.asGLFormatEnum();

  GrBackendRenderTarget render_target(size.width(), size.height(),
                                      characterization.sampleCount(),
                                      /*stencilBits=*/0, framebuffer_info);
  auto origin = (gl_surface_->GetOrigin() == gfx::SurfaceOrigin::kTopLeft)
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gr_context, render_target, origin, color_type,
      characterization.refColorSpace(), &surface_props);
  if (!sk_surface_) {
    LOG(ERROR) << "Couldn't create surface:"
               << "\n  abandoned()=" << gr_context->abandoned()
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

void SkiaOutputDeviceGL::Present(const absl::optional<gfx::Rect>& update_rect,
                                 BufferPresentedCallback feedback,
                                 OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  auto data = frame.data;
  if (supports_async_swap_) {
    auto callback = base::BindOnce(
        &SkiaOutputDeviceGL::DoFinishSwapBuffersAsync,
        weak_ptr_factory_.GetWeakPtr(), surface_size, std::move(frame));

    if (update_rect) {
      gl_surface_->PostSubBufferAsync(
          update_rect->x(), update_rect->y(), update_rect->width(),
          update_rect->height(), std::move(callback), std::move(feedback),
          std::move(data));
    } else {
      gl_surface_->SwapBuffersAsync(std::move(callback), std::move(feedback),
                                    std::move(data));
    }
  } else {
    gfx::SwapResult result;
    if (update_rect) {
      result = gl_surface_->PostSubBuffer(
          update_rect->x(), update_rect->y(), update_rect->width(),
          update_rect->height(), std::move(feedback), std::move(data));
    } else {
      result = gl_surface_->SwapBuffers(std::move(feedback), std::move(data));
    }
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
  FinishSwapBuffers(std::move(result), size, std::move(frame));
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

}  // namespace viz
