// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_gl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gl/color_space_utils.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

namespace viz {

SkiaOutputDeviceGL::SkiaOutputDeviceGL(
    gpu::MailboxManager* mailbox_manager,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback)
    : SkiaOutputDevice(false /*need_swap_semaphore */,
                       did_swap_buffer_complete_callback),
      mailbox_manager_(mailbox_manager),
      gl_surface_(std::move(gl_surface)) {
  capabilities_.flipped_output_surface = gl_surface_->FlipsVertically();
  capabilities_.supports_post_sub_buffer = gl_surface_->SupportsPostSubBuffer();
  if (feature_info->workarounds()
          .disable_post_sub_buffers_for_onscreen_surfaces)
    capabilities_.supports_post_sub_buffer = false;
  capabilities_.max_frames_pending = gl_surface_->GetBufferCount() - 1;
  capabilities_.supports_gpu_vsync = gl_surface_->SupportsGpuVSync();
  capabilities_.supports_dc_layers = gl_surface_->SupportsDCLayers();
  capabilities_.supports_dc_video_overlays = gl_surface_->UseOverlaysForVideo();
#if defined(OS_ANDROID)
  // TODO(weiliangc): This capability is used to check whether we should do
  // overlay. Since currently none of the other overlay system is implemented,
  // only update this for Android.
  // This output device is never offscreen.
  capabilities_.supports_surfaceless = gl_surface_->IsSurfaceless();
#endif
}

void SkiaOutputDeviceGL::Initialize(GrContext* gr_context,
                                    gl::GLContext* gl_context) {
  DCHECK(gr_context);
  DCHECK(gl_context);
  gr_context_ = gr_context;

  gl::CurrentGL* current_gl = gl_context->GetCurrentGL();
  DCHECK(current_gl);

  // Get alpha bits from the default frame buffer.
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  gr_context_->resetContext(kRenderTarget_GrGLBackendState);
  const auto* version = current_gl->Version;
  GLint alpha_bits = 0;
  if (version->is_desktop_core_profile) {
    glGetFramebufferAttachmentParameterivEXT(
        GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
        &alpha_bits);
  } else {
    glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
  }
  CHECK_GL_ERROR();
  supports_alpha_ = alpha_bits > 0;
}

SkiaOutputDeviceGL::~SkiaOutputDeviceGL() {}

bool SkiaOutputDeviceGL::Reshape(const gfx::Size& size,
                                 float device_scale_factor,
                                 const gfx::ColorSpace& color_space,
                                 bool has_alpha,
                                 gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  gl::GLSurface::ColorSpace surface_color_space =
      gl::ColorSpaceUtils::GetGLSurfaceColorSpace(color_space);
  if (!gl_surface_->Resize(size, device_scale_factor, surface_color_space,
                           has_alpha)) {
    DLOG(ERROR) << "Failed to resize.";
    return false;
  }
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = gl_surface_->GetBackingFramebufferObject();
  framebuffer_info.fFormat = supports_alpha_ ? GL_RGBA8 : GL_RGB8_OES;
  GrBackendRenderTarget render_target(size.width(), size.height(), 0, 8,
                                      framebuffer_info);
  auto origin = gl_surface_->FlipsVertically() ? kTopLeft_GrSurfaceOrigin
                                               : kBottomLeft_GrSurfaceOrigin;
  auto color_type =
      supports_alpha_ ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType;
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gr_context_, render_target, origin, color_type,
      color_space.ToSkColorSpace(), &surface_props);
  DCHECK(sk_surface_);
  return true;
}

void SkiaOutputDeviceGL::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  if (gl_surface_->SupportsAsyncSwap()) {
    auto callback = base::BindOnce(&SkiaOutputDeviceGL::DoFinishSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr(), surface_size,
                                   std::move(latency_info));
    gl_surface_->SwapBuffersAsync(std::move(callback), std::move(feedback));
  } else {
    FinishSwapBuffers(gl_surface_->SwapBuffers(std::move(feedback)),
                      surface_size, std::move(latency_info));
  }
}

void SkiaOutputDeviceGL::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  if (gl_surface_->SupportsAsyncSwap()) {
    auto callback = base::BindOnce(&SkiaOutputDeviceGL::DoFinishSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr(), surface_size,
                                   std::move(latency_info));
    gl_surface_->PostSubBufferAsync(rect.x(), rect.y(), rect.width(),
                                    rect.height(), std::move(callback),
                                    std::move(feedback));

  } else {
    FinishSwapBuffers(
        gl_surface_->PostSubBuffer(rect.x(), rect.y(), rect.width(),
                                   rect.height(), std::move(feedback)),
        surface_size, std::move(latency_info));
  }
}

void SkiaOutputDeviceGL::DoFinishSwapBuffers(
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(!gpu_fence);
  FinishSwapBuffers(result, size, latency_info);
}

void SkiaOutputDeviceGL::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  gl_surface_->SetDrawRectangle(draw_rectangle);
}

void SkiaOutputDeviceGL::SetGpuVSyncEnabled(bool enabled) {
  gl_surface_->SetGpuVSyncEnabled(enabled);
}

#if defined(OS_WIN)
void SkiaOutputDeviceGL::SetEnableDCLayers(bool enable) {
  gl_surface_->SetEnableDCLayers(enable);
}

void SkiaOutputDeviceGL::ScheduleDCLayers(
    std::vector<DCLayerOverlay> dc_layers) {
  for (auto& dc_layer : dc_layers) {
    ui::DCRendererLayerParams params;

    // Get GLImages for DC layer textures.
    bool success = true;
    for (size_t i = 0; i < DCLayerOverlay::kNumResources; ++i) {
      if (i > 0 && dc_layer.mailbox[i].IsZero())
        break;

      auto image = GetGLImageForMailbox(dc_layer.mailbox[i]);
      if (!image) {
        success = false;
        break;
      }

      image->SetColorSpace(dc_layer.color_space);
      params.images[i] = std::move(image);
    }

    if (!success) {
      DLOG(ERROR) << "Failed to get GLImage for DC layer.";
      continue;
    }

    params.z_order = dc_layer.z_order;
    params.content_rect = dc_layer.content_rect;
    params.quad_rect = dc_layer.quad_rect;
    DCHECK(dc_layer.transform.IsFlat());
    params.transform = dc_layer.transform;
    params.is_clipped = dc_layer.is_clipped;
    params.clip_rect = dc_layer.clip_rect;
    params.protected_video_type = dc_layer.protected_video_type;

    // Schedule DC layer overlay to be presented at next SwapBuffers().
    if (!gl_surface_->ScheduleDCLayer(params))
      DLOG(ERROR) << "ScheduleDCLayer failed";
  }
}
#endif

void SkiaOutputDeviceGL::EnsureBackbuffer() {
  gl_surface_->SetBackbufferAllocation(true);
}

void SkiaOutputDeviceGL::DiscardBackbuffer() {
  gl_surface_->SetBackbufferAllocation(false);
}

SkSurface* SkiaOutputDeviceGL::BeginPaint() {
  DCHECK(sk_surface_);
  return sk_surface_.get();
}

void SkiaOutputDeviceGL::EndPaint(const GrBackendSemaphore& semaphore) {}

scoped_refptr<gl::GLImage> SkiaOutputDeviceGL::GetGLImageForMailbox(
    const gpu::Mailbox& mailbox) {
  // TODO(crbug.com/1005306): Use SharedImageManager to get textures here once
  // all clients are using SharedImageInterface to create textures.
  auto* texture_base = mailbox_manager_->ConsumeTexture(mailbox);
  if (!texture_base)
    return nullptr;

  if (texture_base->GetType() == gpu::TextureBase::Type::kPassthrough) {
    gpu::gles2::TexturePassthrough* texture =
        static_cast<gpu::gles2::TexturePassthrough*>(texture_base);
    return texture->GetLevelImage(texture->target(), 0);
  } else {
    DCHECK_EQ(texture_base->GetType(), gpu::TextureBase::Type::kValidated);
    gpu::gles2::Texture* texture =
        static_cast<gpu::gles2::Texture*>(texture_base);
    return texture->GetLevelImage(texture->target(), 0);
  }
}

}  // namespace viz
