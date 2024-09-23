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
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

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

class SkiaOutputDeviceGL::MultiSurfaceSwapBuffersTracker {
 public:
  // Returns true if multiple surface swaps detected.
  bool TrackSwapBuffers() {
    static int next_global_swap_generation = 0;
    static int num_swaps_in_current_swap_generation = 0;
    static int last_multi_window_swap_generation = 0;

    // This code is a simple way of enforcing that we only vsync if one surface
    // is swapping per frame. This provides single window cases a stable refresh
    // while allowing multi-window cases to not slow down due to multiple syncs
    // on a single thread. A better way to fix this problem would be to have
    // each surface present on its own thread.

    // If next global swap generation equals to our surface's next swap
    // generation means we start new swap generation and this is first surface
    // to swap.
    if (next_global_swap_generation == next_surface_swap_generation_) {
      // Start new generation.
      next_global_swap_generation++;

      // Store number of swaps in the previous generation.
      if (num_swaps_in_current_swap_generation > 1) {
        last_multi_window_swap_generation = next_global_swap_generation;
      }
      num_swaps_in_current_swap_generation = 0;
    }

    next_surface_swap_generation_ = next_global_swap_generation;
    num_swaps_in_current_swap_generation++;

    // Number of swap generations before vsync is re-enabled after we've stopped
    // doing multiple swaps per frame.
    constexpr int kMultiWindowSwapEnableVSyncDelay = 60;

    return (num_swaps_in_current_swap_generation > 1) ||
           (next_global_swap_generation - last_multi_window_swap_generation <
            kMultiWindowSwapEnableVSyncDelay);
  }

 private:
  int next_surface_swap_generation_ = 0;
};

SkiaOutputDeviceGL::SkiaOutputDeviceGL(
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       context_state->graphite_context(),
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
  capabilities_.pending_swap_params.max_pending_swaps =
      gl_surface_->GetBufferCount() - 1;
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

  DCHECK(context_state_->gr_context());
  DCHECK(context_state_->context());

  GrDirectContext* gr_context = context_state_->gr_context();

  // Get alpha bits from the default frame buffer.
  int alpha_bits = 0;
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  gr_context->resetContext(kRenderTarget_GrGLBackendState);
  glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
  CHECK_GL_ERROR();

  auto color_type = kRGBA_8888_SkColorType;

  if (alpha_bits == 0) {
    color_type = gl_surface_->GetFormat().IsRGB565() ? kRGB_565_SkColorType
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
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_8888] = color_type;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBX_8888] = color_type;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRA_8888] = color_type;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRX_8888] = color_type;
  // HDR10
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_1010102] =
      kRGBA_1010102_SkColorType;
  // scRGB linear
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_F16] =
      kRGBA_F16_SkColorType;

  if (features::UseGpuVsync()) {
    // Historically we never disabled vsync on Android and it's very rare
    // use-case to have multiple active windows there. On other platforms we
    // disable GLSurface's VSync if we're swapping multiple surfaces per frame
    // to prevent SwapBuffers from blocking and slowing down other windows.
#if !BUILDFLAG(IS_ANDROID)
    multisurface_swapbuffers_tracker_ =
        std::make_unique<MultiSurfaceSwapBuffersTracker>();
#endif
  } else {
    gl_surface_->SetVSyncEnabled(false);
  }
}

SkiaOutputDeviceGL::~SkiaOutputDeviceGL() {
  // gl_surface_ will be destructed soon.
  memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
}

bool SkiaOutputDeviceGL::Reshape(const ReshapeParams& params) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK_EQ(params.transform, gfx::OVERLAY_TRANSFORM_NONE);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  const gfx::Size size = params.GfxSize();
  const SkColorType color_type = params.image_info.colorType();
  const bool has_alpha = !params.image_info.isOpaque();

  if (!gl_surface_->Resize(size, params.device_scale_factor, params.color_space,
                           has_alpha)) {
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }
  SkSurfaceProps surface_props;

  GrGLFramebufferInfo framebuffer_info = {0};
  DCHECK_EQ(gl_surface_->GetBackingFramebufferObject(), 0u);

  auto* gr_context = context_state_->gr_context();

  GrBackendFormat backend_format =
      gr_context->defaultBackendFormat(color_type, GrRenderable::kYes);
  DCHECK(backend_format.isValid()) << "color_type: " << color_type;
  framebuffer_info.fFormat = GrBackendFormats::AsGLFormatEnum(backend_format);

  auto render_target = GrBackendRenderTargets::MakeGL(
      size.width(), size.height(), params.sample_count,
      /*stencilBits=*/0, framebuffer_info);
  auto origin = (gl_surface_->GetOrigin() == gfx::SurfaceOrigin::kTopLeft)
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;
  sk_surface_ = SkSurfaces::WrapBackendRenderTarget(
      gr_context, render_target, origin, color_type,
      params.image_info.refColorSpace(), &surface_props);
  if (!sk_surface_) {
    LOG(ERROR) << "Couldn't create surface:" << "\n  abandoned()="
               << gr_context->abandoned() << "\n  color_type=" << color_type
               << "\n  framebuffer_info.fFBOID=" << framebuffer_info.fFBOID
               << "\n  framebuffer_info.fFormat=" << framebuffer_info.fFormat
               << "\n  color_space=" << params.color_space.ToString()
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

void SkiaOutputDeviceGL::Present(const std::optional<gfx::Rect>& update_rect,
                                 BufferPresentedCallback feedback,
                                 OutputSurfaceFrame frame) {
  if (multisurface_swapbuffers_tracker_) {
    const bool multiple_surface_swaps =
        multisurface_swapbuffers_tracker_->TrackSwapBuffers();
    gl_surface_->SetVSyncEnabled(!multiple_surface_swaps);
  }

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

SkSurface* SkiaOutputDeviceGL::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(sk_surface_);
  return sk_surface_.get();
}

void SkiaOutputDeviceGL::EndPaint() {}

}  // namespace viz
