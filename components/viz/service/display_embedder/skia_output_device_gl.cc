// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_gl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

namespace viz {

SkiaOutputDeviceGL::SkiaOutputDeviceGL(
    gpu::MailboxManager* mailbox_manager,
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      mailbox_manager_(mailbox_manager),
      context_state_(context_state),
      gl_surface_(std::move(gl_surface)),
      supports_async_swap_(gl_surface_->SupportsAsyncSwap()) {
  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gl_surface_->GetOrigin();
  capabilities_.supports_post_sub_buffer = gl_surface_->SupportsPostSubBuffer();
#if defined(OS_WIN)
  if (gl_surface_->SupportsDCLayers() &&
      gl::ShouldForceDirectCompositionRootSurfaceFullDamage()) {
    // We need to set this bit to allow viz to track the previous damage rect
    // of a backbuffer in a multiple backbuffer system, so backbuffers always
    // have valid pixels, even outside the current damage rect.
    capabilities_.preserve_buffer_content = true;
  }
#endif  // OS_WIN
  if (feature_info->workarounds()
          .disable_post_sub_buffers_for_onscreen_surfaces) {
    capabilities_.supports_post_sub_buffer = false;
  }
  if (feature_info->workarounds().force_rgb10a2_overlay_support_flags) {
    capabilities_.forces_rgb10a2_overlay_support_flags = true;
  }
  capabilities_.max_frames_pending = gl_surface_->GetBufferCount() - 1;
  capabilities_.supports_commit_overlay_planes =
      gl_surface_->SupportsCommitOverlayPlanes();
  capabilities_.supports_gpu_vsync = gl_surface_->SupportsGpuVSync();
  capabilities_.supports_dc_layers = gl_surface_->SupportsDCLayers();
#if defined(OS_ANDROID)
  // TODO(weiliangc): This capability is used to check whether we should do
  // overlay. Since currently none of the other overlay system is implemented,
  // only update this for Android.
  // This output device is never offscreen.
  capabilities_.supports_surfaceless = gl_surface_->IsSurfaceless();
#endif

  DCHECK(context_state_->gr_context());
  DCHECK(context_state_->context());

  if (gl_surface_->SupportsSwapTimestamps()) {
    gl_surface_->SetEnableSwapTimestamps();

    // Changes to swap timestamp queries are only picked up when making current.
    context_state_->ReleaseCurrent(nullptr);
    context_state_->MakeCurrent(gl_surface_.get());
  }

  GrContext* gr_context = context_state_->gr_context();
  gl::CurrentGL* current_gl = context_state_->context()->GetCurrentGL();

  // Get alpha bits from the default frame buffer.
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  gr_context->resetContext(kRenderTarget_GrGLBackendState);
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

  capabilities_.sk_color_type =
      supports_alpha_ ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType;
  capabilities_.gr_backend_format = gr_context->defaultBackendFormat(
      capabilities_.sk_color_type, GrRenderable::kYes);
  capabilities_.sk_color_type_for_hdr = kRGBA_F16_SkColorType;
  capabilities_.gr_backend_format_for_hdr = gr_context->defaultBackendFormat(
      capabilities_.sk_color_type_for_hdr, GrRenderable::kYes);
}

SkiaOutputDeviceGL::~SkiaOutputDeviceGL() {
  // gl_surface_ will be destructed soon.
  memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
}

bool SkiaOutputDeviceGL::Reshape(const gfx::Size& size,
                                 float device_scale_factor,
                                 const gfx::ColorSpace& color_space,
                                 gfx::BufferFormat buffer_format,
                                 gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  if (!gl_surface_->Resize(size, device_scale_factor, color_space,
                           gfx::AlphaBitsForBufferFormat(buffer_format))) {
    DLOG(ERROR) << "Failed to resize.";
    return false;
  }
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = 0;
  DCHECK_EQ(gl_surface_->GetBackingFramebufferObject(), 0u);

  SkColorType color_type;
  // TODO(https://crbug.com/1049334): The pixel format should be determined by
  // |buffer_format|, not |color_space|, and not |supports_alpha_|.
  if (color_space.IsHDR()) {
    color_type = capabilities_.sk_color_type_for_hdr;
    framebuffer_info.fFormat = GL_RGBA16F;
    DCHECK_EQ(capabilities_.gr_backend_format_for_hdr.asGLFormat(),
              GrGLFormat::kRGBA16F);
  } else if (supports_alpha_) {
    color_type = capabilities_.sk_color_type;
    framebuffer_info.fFormat = GL_RGBA8;
    DCHECK_EQ(capabilities_.gr_backend_format.asGLFormat(), GrGLFormat::kRGBA8);
  } else {
    color_type = capabilities_.sk_color_type;
    framebuffer_info.fFormat = GL_RGB8;
    DCHECK_EQ(capabilities_.gr_backend_format.asGLFormat(), GrGLFormat::kRGB8);
  }
  // TODO(kylechar): We might need to support RGB10A2 for HDR10. HDR10 was only
  // used with Windows updated RS3 (2017) as a workaround for a DWM bug so it
  // might not be relevant to support anymore as a result.

  GrBackendRenderTarget render_target(size.width(), size.height(),
                                      /*sampleCnt=*/0,
                                      /*stencilBits=*/0, framebuffer_info);
  auto origin = (gl_surface_->GetOrigin() == gfx::SurfaceOrigin::kTopLeft)
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      context_state_->gr_context(), render_target, origin, color_type,
      color_space.ToSkColorSpace(), &surface_props);
  if (!sk_surface_) {
    LOG(ERROR) << "Couldn't create surface: "
               << context_state_->gr_context()->abandoned() << " " << color_type
               << " " << framebuffer_info.fFBOID << " "
               << framebuffer_info.fFormat << " " << color_space.ToString()
               << " " << size.ToString();
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

void SkiaOutputDeviceGL::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  if (supports_async_swap_) {
    auto callback = base::BindOnce(&SkiaOutputDeviceGL::DoFinishSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr(), surface_size,
                                   std::move(latency_info));
    gl_surface_->SwapBuffersAsync(std::move(callback), std::move(feedback));
  } else {
    gfx::SwapResult result = gl_surface_->SwapBuffers(std::move(feedback));
    FinishSwapBuffers(gfx::SwapCompletionResult(result), surface_size,
                      std::move(latency_info));
  }
}

void SkiaOutputDeviceGL::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  if (supports_async_swap_) {
    auto callback = base::BindOnce(&SkiaOutputDeviceGL::DoFinishSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr(), surface_size,
                                   std::move(latency_info));
    gl_surface_->PostSubBufferAsync(rect.x(), rect.y(), rect.width(),
                                    rect.height(), std::move(callback),
                                    std::move(feedback));
  } else {
    gfx::SwapResult result = gl_surface_->PostSubBuffer(
        rect.x(), rect.y(), rect.width(), rect.height(), std::move(feedback));
    FinishSwapBuffers(gfx::SwapCompletionResult(result), surface_size,
                      std::move(latency_info));
  }
}

void SkiaOutputDeviceGL::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  if (supports_async_swap_) {
    auto callback = base::BindOnce(&SkiaOutputDeviceGL::DoFinishSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr(), surface_size,
                                   std::move(latency_info));
    gl_surface_->CommitOverlayPlanesAsync(std::move(callback),
                                          std::move(feedback));
  } else {
    FinishSwapBuffers(
        gfx::SwapCompletionResult(
            gl_surface_->CommitOverlayPlanes(std::move(feedback))),
        surface_size, std::move(latency_info));
  }
}

void SkiaOutputDeviceGL::DoFinishSwapBuffers(
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    gfx::SwapCompletionResult result) {
  DCHECK(!result.gpu_fence);
  FinishSwapBuffers(std::move(result), size, latency_info);
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
#if defined(OS_WIN)
  for (auto& dc_layer : overlays) {
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
#endif  // OS_WIN
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
