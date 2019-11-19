// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue.h"

#include <utility>

#include "base/bind.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/buffer_queue.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gl/gl_enums.h"

namespace viz {

GLOutputSurfaceBufferQueue::GLOutputSurfaceBufferQueue(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferFormat buffer_format)
    : GLOutputSurface(context_provider, surface_handle),
      current_texture_(0u),
      fbo_(0u) {
  capabilities_.only_invalidates_damage_rect = false;
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.flipped_output_surface = true;
  // Set |max_frames_pending| to 2 for buffer_queue, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // BufferQueue implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;

  buffer_queue_ = std::make_unique<BufferQueue>(
      context_provider->ContextGL(), buffer_format, gpu_memory_buffer_manager,
      surface_handle, context_provider->ContextCapabilities());
  context_provider_->ContextGL()->GenFramebuffers(1, &fbo_);
}

GLOutputSurfaceBufferQueue::~GLOutputSurfaceBufferQueue() {
  DCHECK_NE(0u, fbo_);
  context_provider_->ContextGL()->DeleteFramebuffers(1, &fbo_);
}

void GLOutputSurfaceBufferQueue::BindFramebuffer() {
  auto* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  DCHECK(buffer_queue_);
  unsigned stencil;
  current_texture_ = buffer_queue_->GetCurrentBuffer(&stencil);
  if (!current_texture_)
    return;
  // TODO(andrescj): if the texture hasn't changed since the last call to
  // BindFrameBuffer(), we may be able to avoid mutating the FBO which may lead
  // to performance improvements.
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           buffer_queue_->texture_target(), current_texture_,
                           0);

#if DCHECK_IS_ON() && defined(OS_CHROMEOS)
  const GLenum result = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
  if (result != GL_FRAMEBUFFER_COMPLETE)
    DLOG(ERROR) << " Incomplete fb: " << gl::GLEnums::GetStringError(result);
#endif

  if (stencil) {
    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, stencil);
  }
}

// We call this on every frame that a value changes, but changing the size once
// we've allocated backing NativePixmapBufferQueue instances will cause a DCHECK
// because Chrome never Reshape(s) after the first one from (0,0). NB: this
// implies that screen size changes need to be plumbed differently. In
// particular, we must create the native window in the size that the hardware
// reports.
void GLOutputSurfaceBufferQueue::Reshape(const gfx::Size& size,
                                         float device_scale_factor,
                                         const gfx::ColorSpace& color_space,
                                         bool has_alpha,
                                         bool use_stencil) {
  reshape_size_ = size;
  GLOutputSurface::Reshape(size, device_scale_factor, color_space, has_alpha,
                           use_stencil);
  if (buffer_queue_->Reshape(size, device_scale_factor, color_space,
                             use_stencil)) {
    auto* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             buffer_queue_->texture_target(), 0, 0);
    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, 0);
  }
}

void GLOutputSurfaceBufferQueue::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(buffer_queue_);

  // TODO(rjkroege): What if swap happens again before DidReceiveSwapBuffersAck
  // then it would see the wrong size?
  DCHECK(reshape_size_ == frame.size);
  swap_size_ = reshape_size_;

  gfx::Rect damage_rect =
      frame.sub_buffer_rect ? *frame.sub_buffer_rect : gfx::Rect(swap_size_);
  buffer_queue_->SwapBuffers(damage_rect);

  GLOutputSurface::SwapBuffers(std::move(frame));
}

gfx::Rect GLOutputSurfaceBufferQueue::GetCurrentFramebufferDamage() const {
  return buffer_queue_->CurrentBufferDamage();
}

uint32_t GLOutputSurfaceBufferQueue::GetFramebufferCopyTextureFormat() {
  return buffer_queue_->internal_format();
}

bool GLOutputSurfaceBufferQueue::IsDisplayedAsOverlayPlane() const {
  return true;
}

unsigned GLOutputSurfaceBufferQueue::GetOverlayTextureId() const {
  DCHECK(current_texture_);
  return current_texture_;
}

gfx::BufferFormat GLOutputSurfaceBufferQueue::GetOverlayBufferFormat() const {
  DCHECK(buffer_queue_);
  return buffer_queue_->buffer_format();
}

void GLOutputSurfaceBufferQueue::DidReceiveSwapBuffersAck(
    const gfx::SwapResponse& response) {
  bool force_swap = false;
  if (response.result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    buffer_queue_->FreeAllSurfaces();
    force_swap = true;
  }

  buffer_queue_->PageFlipComplete();
  client()->DidReceiveSwapBuffersAck(response.timings);

  if (force_swap)
    client()->SetNeedsRedrawRect(gfx::Rect(swap_size_));
}

void GLOutputSurfaceBufferQueue::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;

  if (context_provider_)
    context_provider_->ContextSupport()->SetDisplayTransform(transform);
}

gfx::OverlayTransform GLOutputSurfaceBufferQueue::GetDisplayTransform() {
  return display_transform_;
}

}  // namespace viz
