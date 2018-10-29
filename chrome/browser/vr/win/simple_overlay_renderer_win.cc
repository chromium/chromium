// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dxgi1_2.h>

#include "chrome/browser/vr/win/simple_overlay_renderer_win.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace vr {

SimpleOverlayRenderer::SimpleOverlayRenderer() {}
SimpleOverlayRenderer::~SimpleOverlayRenderer() {}

bool SimpleOverlayRenderer::InitializeOnMainThread() {
  gpu::GpuChannelEstablishFactory* factory =
      content::GetGpuChannelEstablishFactory();
  scoped_refptr<gpu::GpuChannelHost> host = factory->EstablishGpuChannelSync();

  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.red_size = 8;
  attributes.green_size = 8;
  attributes.blue_size = 8;
  attributes.stencil_size = 0;
  attributes.depth_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;

  context_provider_ = base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
      host, factory->GetGpuMemoryBufferManager(), content::kGpuStreamIdDefault,
      content::kGpuStreamPriorityUI, gpu::kNullSurfaceHandle,
      GURL(std::string("chrome://gpu/SimpleOverlayRendererWin")),
      false /* automatic flushes */, false /* support locking */,
      false /* support grcontext */,
      gpu::SharedMemoryLimits::ForMailboxContext(), attributes,
      ws::command_buffer_metrics::ContextType::XR_COMPOSITING);
  gpu_memory_buffer_manager_ = factory->GetGpuMemoryBufferManager();
  return true;
}

void SimpleOverlayRenderer::InitializeOnGLThread() {
  DCHECK(context_provider_);
  if (context_provider_->BindToCurrentThread() == gpu::ContextResult::kSuccess)
    gl_ = context_provider_->ContextGL();
}

void SimpleOverlayRenderer::Render() {
  if (!gl_)
    return;

  int width = 512;
  int height = 512;

  // Create a memory buffer, and an image referencing that memory buffer.
  if (!EnsureMemoryBuffer(width, height))
    return;

  // Create a texture id, and associate it with our image.
  GLuint dest_texture_id;
  gl_->GenTextures(1, &dest_texture_id);
  GLenum target = GL_TEXTURE_2D;
  gl_->BindTexture(target, dest_texture_id);
  gl_->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->BindTexImage2DCHROMIUM(target, image_id_);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Bind our image/texture/memory buffer as the draw framebuffer.
  GLuint draw_frame_buffer_;
  gl_->GenFramebuffers(1, &draw_frame_buffer_);
  gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer_);
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            dest_texture_id, 0);

  // Do some drawing.
  gl_->ClearColor(0, 1, 0, 0.5f);
  gl_->Clear(GL_COLOR_BUFFER_BIT);

  // Unbind the drawing buffer.
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl_->DeleteFramebuffers(1, &draw_frame_buffer_);
  gl_->BindTexture(target, dest_texture_id);
  gl_->ReleaseTexImage2DCHROMIUM(target, image_id_);
  gl_->DeleteTextures(1, &dest_texture_id);
  gl_->BindTexture(target, 0);

  // Flush.
  gl_->ShallowFlushCHROMIUM();
}

mojo::ScopedHandle SimpleOverlayRenderer::GetTexture() {
  // Hand out the gpu memory buffer.
  mojo::ScopedHandle handle;
  if (!gpu_memory_buffer_) {
    return handle;
  }

  gfx::GpuMemoryBufferHandle gpu_handle = gpu_memory_buffer_->CloneHandle();
  return mojo::WrapPlatformFile(gpu_handle.dxgi_handle.GetHandle());
}

gfx::RectF SimpleOverlayRenderer::GetLeft() {
  return gfx::RectF(0, 0, 0.5, 1);
}

gfx::RectF SimpleOverlayRenderer::GetRight() {
  return gfx::RectF(0.5, 0, 0.5, 1);
}

void SimpleOverlayRenderer::Cleanup() {
  context_provider_ = nullptr;
}

bool SimpleOverlayRenderer::EnsureMemoryBuffer(int width, int height) {
  if (last_width_ != width || last_height_ != height || !gpu_memory_buffer_) {
    if (!gpu_memory_buffer_manager_)
      return false;

    if (image_id_) {
      gl_->DestroyImageCHROMIUM(image_id_);
      image_id_ = 0;
    }

    gpu_memory_buffer_ = gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
        gfx::Size(width, height), gfx::BufferFormat::RGBA_8888,
        gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle);
    if (!gpu_memory_buffer_)
      return false;

    last_width_ = width;
    last_height_ = height;

    image_id_ = gl_->CreateImageCHROMIUM(gpu_memory_buffer_->AsClientBuffer(),
                                         width, height, GL_RGBA);
    if (!image_id_) {
      gpu_memory_buffer_ = nullptr;
      return false;
    }
  }
  return true;
}

void SimpleOverlayRenderer::ResetMemoryBuffer() {
  // Stop using a memory buffer if we had an error submitting with it.
  gpu_memory_buffer_ = nullptr;
}

}  // namespace vr
