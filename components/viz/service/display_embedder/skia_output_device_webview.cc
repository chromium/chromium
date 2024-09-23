// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_webview.h"

#include <utility>

#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface.h"

namespace viz {

namespace {
constexpr auto kSurfaceColorType = kRGBA_8888_SkColorType;
}

SkiaOutputDeviceWebView::SkiaOutputDeviceWebView(
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::GLSurface> gl_surface,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       context_state->graphite_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      context_state_(context_state),
      gl_surface_(std::move(gl_surface)) {
  // Always set uses_default_gl_framebuffer to true, since
  // GrSurfaceCharacterization created for  GL fbo0 is compatible with
  // SkSurface wrappers non GL fbo0.
  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gl_surface_->GetOrigin();
  capabilities_.pending_swap_params.max_pending_swaps =
      gl_surface_->GetBufferCount() - 1;

  DCHECK(context_state_->gr_context());
  DCHECK(context_state_->context());

  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_8888] =
      kSurfaceColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRA_8888] =
      kSurfaceColorType;
}

SkiaOutputDeviceWebView::~SkiaOutputDeviceWebView() = default;

bool SkiaOutputDeviceWebView::Reshape(const ReshapeParams& params) {
  DCHECK_EQ(params.transform, gfx::OVERLAY_TRANSFORM_NONE);

  gfx::Size size = params.GfxSize();
  if (!gl_surface_->Resize(size, params.device_scale_factor, params.color_space,
                           /*has_alpha=*/true)) {
    DLOG(ERROR) << "Failed to resize.";
    return false;
  }

  size_ = size;
  sk_color_space_ = params.image_info.refColorSpace();
  InitSkiaSurface(gl_surface_->GetBackingFramebufferObject());
  return !!sk_surface_;
}

void SkiaOutputDeviceWebView::Present(
    const std::optional<gfx::Rect>& update_rect,
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
  DCHECK(!update_rect);
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  auto data = frame.data;
  FinishSwapBuffers(gfx::SwapCompletionResult(
                        gl_surface_->SwapBuffers(std::move(feedback), data)),
                    surface_size, std::move(frame));
}

SkSurface* SkiaOutputDeviceWebView::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(sk_surface_);

  unsigned int fbo = gl_surface_->GetBackingFramebufferObject();

  if (last_frame_buffer_object_ != fbo) {
    InitSkiaSurface(fbo);
  }

  return sk_surface_.get();
}

void SkiaOutputDeviceWebView::EndPaint() {}

void SkiaOutputDeviceWebView::InitSkiaSurface(unsigned int fbo) {
  last_frame_buffer_object_ = fbo;

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = fbo;
  framebuffer_info.fFormat = GL_RGBA8;
  SkColorType color_type = kSurfaceColorType;

  auto render_target =
      GrBackendRenderTargets::MakeGL(size_.width(), size_.height(),
                                     /*sampleCnt=*/0,
                                     /*stencilBits=*/0, framebuffer_info);
  auto origin = (gl_surface_->GetOrigin() == gfx::SurfaceOrigin::kTopLeft)
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;

  SkSurfaceProps surface_props;
  sk_surface_ = SkSurfaces::WrapBackendRenderTarget(
      context_state_->gr_context(), render_target, origin, color_type,
      sk_color_space_, &surface_props);

  if (!sk_surface_) {
    LOG(ERROR) << "Couldn't create surface: "
               << context_state_->gr_context()->abandoned() << " " << color_type
               << " " << framebuffer_info.fFBOID << " "
               << framebuffer_info.fFormat << " " << size_.ToString();
  }
}

}  // namespace viz
