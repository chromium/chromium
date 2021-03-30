// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_enums.h"

namespace viz {

GLOutputSurfaceBufferQueue::GLOutputSurfaceBufferQueue(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle,
    std::unique_ptr<BufferQueue> buffer_queue)
    : GLOutputSurface(context_provider, surface_handle),
      buffer_queue_(std::move(buffer_queue)) {
  capabilities_.only_invalidates_damage_rect = false;
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  // Set |max_frames_pending| to 2 for buffer_queue, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // BufferQueue implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;
  // GetCurrentFramebufferDamage will return an upper bound of the part of the
  // buffer that needs to be recomposited.
#if defined(OS_APPLE)
  capabilities_.supports_target_damage = false;
#else
  capabilities_.supports_target_damage = true;
#endif
  // Force the number of max pending frames to one when the switch
  // "double-buffer-compositing" is passed.
  // This will keep compositing in double buffered mode assuming |buffer_queue_|
  // allocates at most one additional buffer.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDoubleBufferCompositing)) {
    capabilities_.max_frames_pending = 1;
    buffer_queue_->SetMaxBuffers(2);
  }

  // It is safe to pass a raw pointer to *this because |buffer_queue_| is fully
  // owned and it doesn't use the SyncTokenProvider after it's destroyed.
  DCHECK(buffer_queue_);
  buffer_queue_->SetSyncTokenProvider(this);
  context_provider_->ContextGL()->GenFramebuffers(1, &fbo_);
}

GLOutputSurfaceBufferQueue::~GLOutputSurfaceBufferQueue() {
  auto* gl = context_provider_->ContextGL();
  DCHECK_NE(0u, fbo_);
  gl->DeleteFramebuffers(1, &fbo_);
  if (stencil_buffer_)
    gl->DeleteRenderbuffers(1, &stencil_buffer_);
  for (const auto& buffer_texture : buffer_queue_textures_)
    gl->DeleteTextures(1u, &buffer_texture.second);
  buffer_queue_textures_.clear();
  current_texture_ = 0u;
  last_bound_texture_ = 0u;
  last_bound_mailbox_.SetZero();

  // Freeing the BufferQueue here ensures that *this is fully alive in case the
  // BufferQueue needs the SyncTokenProvider functionality.
  buffer_queue_.reset();
  fbo_ = 0u;
  stencil_buffer_ = 0u;
}

void GLOutputSurfaceBufferQueue::BindFramebuffer() {
  auto* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);

  // If we have a |current_texture_|, it means we haven't swapped the buffer, so
  // we're just wanting to rebind the GL framebuffer.
  if (current_texture_)
    return;

  DCHECK(buffer_queue_);
  gpu::SyncToken creation_sync_token;
  const gpu::Mailbox current_buffer =
      buffer_queue_->GetCurrentBuffer(&creation_sync_token);
  if (current_buffer.IsZero())
    return;
  gl->WaitSyncTokenCHROMIUM(creation_sync_token.GetConstData());
  unsigned& buffer_texture = buffer_queue_textures_[current_buffer];
  if (!buffer_texture) {
    buffer_texture =
        gl->CreateAndTexStorage2DSharedImageCHROMIUM(current_buffer.name);
  }
  current_texture_ = buffer_texture;
  gl->BeginSharedImageAccessDirectCHROMIUM(
      current_texture_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           texture_target_, current_texture_, 0);
  last_bound_texture_ = current_texture_;
  last_bound_mailbox_ = current_buffer;

#if DCHECK_IS_ON() && BUILDFLAG(IS_CHROMEOS_ASH)
  const GLenum result = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
  if (result != GL_FRAMEBUFFER_COMPLETE)
    DLOG(ERROR) << " Incomplete fb: " << gl::GLEnums::GetStringError(result);
#endif

  // Reshape() must be called to go from using a stencil buffer to not using it.
  DCHECK(use_stencil_ || !stencil_buffer_);
  if (use_stencil_ && !stencil_buffer_) {
    gl->GenRenderbuffers(1, &stencil_buffer_);
    CHECK_NE(stencil_buffer_, 0u);
    gl->BindRenderbuffer(GL_RENDERBUFFER, stencil_buffer_);
    gl->RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                            reshape_size_.width(), reshape_size_.height());
    gl->BindRenderbuffer(GL_RENDERBUFFER, 0);
    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, stencil_buffer_);
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
                                         gfx::BufferFormat format,
                                         bool use_stencil) {
  reshape_size_ = size;
  use_stencil_ = use_stencil;
  GLOutputSurface::Reshape(size, device_scale_factor, color_space, format,
                           use_stencil);
  DCHECK(buffer_queue_);
  const bool may_have_freed_buffers =
      buffer_queue_->Reshape(size, color_space, format);
  if (may_have_freed_buffers || (stencil_buffer_ && !use_stencil)) {
    auto* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    if (stencil_buffer_) {
      gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, 0);
      gl->DeleteRenderbuffers(1, &stencil_buffer_);
      stencil_buffer_ = 0u;
    }

    // Note that |texture_target_| is initially set to 0, and so if it has not
    // been set to a valid value, then no buffers have been allocated.
    if (texture_target_ && may_have_freed_buffers) {
      gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               texture_target_, 0, 0);
      for (const auto& buffer_texture : buffer_queue_textures_)
        gl->DeleteTextures(1u, &buffer_texture.second);
      buffer_queue_textures_.clear();
      current_texture_ = 0u;
      last_bound_texture_ = 0u;
      last_bound_mailbox_.SetZero();
    }
  }

  texture_target_ =
      gpu::GetBufferTextureTarget(gfx::BufferUsage::SCANOUT, format,
                                  context_provider_->ContextCapabilities());
}

void GLOutputSurfaceBufferQueue::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(buffer_queue_);

  // TODO(rjkroege): What if swap happens again before DidReceiveSwapBuffersAck
  // then it would see the wrong size?
  DCHECK(reshape_size_ == frame.size);
  swap_size_ = reshape_size_;

  gfx::Rect damage_rect =
      frame.sub_buffer_rect ? *frame.sub_buffer_rect : gfx::Rect(swap_size_);

  // If the client is currently drawing, we first end access to the
  // corresponding shared image. Then, we can swap the buffers. That way, we
  // know that whatever GL commands GLOutputSurface::SwapBuffers() emits can
  // access the shared image.
  auto* gl = context_provider_->ContextGL();
  if (current_texture_) {
    gl->EndSharedImageAccessDirectCHROMIUM(current_texture_);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0u);
    current_texture_ = 0u;
  }
  buffer_queue_->SwapBuffers(damage_rect);
  GLOutputSurface::SwapBuffers(std::move(frame));
}

gfx::Rect GLOutputSurfaceBufferQueue::GetCurrentFramebufferDamage() const {
  return buffer_queue_->CurrentBufferDamage();
}

uint32_t GLOutputSurfaceBufferQueue::GetFramebufferCopyTextureFormat() {
  return base::strict_cast<GLenum>(
      gl::BufferFormatToGLInternalFormat(buffer_queue_->buffer_format()));
}

bool GLOutputSurfaceBufferQueue::IsDisplayedAsOverlayPlane() const {
  return true;
}

unsigned GLOutputSurfaceBufferQueue::GetOverlayTextureId() const {
  DCHECK(last_bound_texture_);
  return last_bound_texture_;
}

gpu::Mailbox GLOutputSurfaceBufferQueue::GetOverlayMailbox() const {
  return last_bound_mailbox_;
}

void GLOutputSurfaceBufferQueue::DidReceiveSwapBuffersAck(
    const gfx::SwapResponse& response) {
  bool force_swap = false;
  if (response.result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    buffer_queue_->FreeAllSurfaces();

    // TODO(andrescj): centralize the logic that deletes the stencil buffer and
    // the textures since we do this in multiple places.
    auto* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    if (stencil_buffer_) {
      gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, 0);
      gl->DeleteRenderbuffers(1, &stencil_buffer_);
      stencil_buffer_ = 0u;
    }

    // Reshape() must have been called before we got here, so |texture_target_|
    // should contain a valid value.
    DCHECK(texture_target_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             texture_target_, 0, 0);
    for (const auto& buffer_texture : buffer_queue_textures_)
      gl->DeleteTextures(1u, &buffer_texture.second);
    buffer_queue_textures_.clear();
    current_texture_ = 0u;
    last_bound_texture_ = 0u;
    last_bound_mailbox_.SetZero();

    force_swap = true;
  }

  buffer_queue_->PageFlipComplete();
  client()->DidReceiveSwapBuffersAck(response.timings);

  if (force_swap)
    client()->SetNeedsRedrawRect(gfx::Rect(swap_size_));
}

gpu::SyncToken GLOutputSurfaceBufferQueue::GenSyncToken() {
  // This should only be called as long as the BufferQueue is alive. We cannot
  // use |buffer_queue_| to detect this because in the dtor, |buffer_queue_|
  // becomes nullptr before BufferQueue's dtor is called, so GenSyncToken()
  // would be called after |buffer_queue_| is nullptr when in fact, the
  // BufferQueue is still alive. Hence, we use |fbo_| to detect that the
  // BufferQueue is still alive.
  DCHECK(fbo_);
  gpu::SyncToken sync_token;
  context_provider_->ContextGL()->GenUnverifiedSyncTokenCHROMIUM(
      sync_token.GetData());
  return sync_token;
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
