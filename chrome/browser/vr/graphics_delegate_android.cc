// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/graphics_delegate_android.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/ipc/common/android/android_hardware_buffer_utils.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

GraphicsDelegateAndroid::BufferSlot::BufferSlot() = default;
GraphicsDelegateAndroid::BufferSlot::~BufferSlot() = default;

GraphicsDelegateAndroid::GraphicsDelegateAndroid() = default;
GraphicsDelegateAndroid::~GraphicsDelegateAndroid() {
  ResetMemoryBuffer();
  for (BufferSlot& slot : buffers_) {
    if (slot.draw_frame_buffer != 0) {
      glDeleteFramebuffersEXT(1, &slot.draw_frame_buffer);
      slot.draw_frame_buffer = 0;
    }
    if (slot.shared_buffer && slot.shared_buffer->local_texture.id != 0) {
      glDeleteTextures(1, &slot.shared_buffer->local_texture.id);
      slot.shared_buffer->local_texture.id = 0;
    }
  }
}

void GraphicsDelegateAndroid::Initialize(base::OnceClosure on_initialized) {
  // We can only share native GL resources with the runtimes, and they don't
  // support ANGLE, so disable it.
  // TODO(crbug.com/40744597): support ANGLE?
  gl::DisableANGLE();

  gl::GLDisplay* display = nullptr;
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    display = gl::init::InitializeGLOneOff(
        /*gpu_preference=*/gl::GpuPreference::kDefault);
    if (!display) {
      LOG(ERROR) << "gl::init::InitializeGLOneOff failed";
      return;
    }
  } else {
    display = gl::GetDefaultDisplayEGL();
  }

  surface_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());
  if (!surface_.get()) {
    LOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    return;
  }

  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    DLOG(ERROR) << "gl::init::CreateGLContext failed";
    return;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    DLOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return;
  }

  mailbox_bridge_ = std::make_unique<webxr::MailboxToSurfaceBridgeImpl>();
  mailbox_bridge_->CreateAndBindContextProvider(base::BindOnce(
      &GraphicsDelegateAndroid::OnMailboxBridgeReady,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_initialized)));
}

void GraphicsDelegateAndroid::OnMailboxBridgeReady(
    base::OnceClosure on_initialized) {
  DCHECK(mailbox_bridge_->IsConnected());
  std::move(on_initialized).Run();
}

bool GraphicsDelegateAndroid::BindContext() {
  return true;
}

void GraphicsDelegateAndroid::ClearContext() {}

bool GraphicsDelegateAndroid::PreRender() {
  BindContext();

  current_buffer_index_ = (current_buffer_index_ + 1) % kNumBuffers;

  // Create and bind a memory buffer for rendering the overlay.
  if (!EnsureAndBindMemoryBuffer()) {
    DLOG(ERROR) << __func__ << " Failed to ensure memory buffer";
    return false;
  }

  return true;
}

void GraphicsDelegateAndroid::PostRender() {
  // Unbind the drawing buffer.
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

  glBindTexture(GL_TEXTURE_2D, 0);

  ClearContext();
  glFlush();
}

scoped_refptr<gpu::ClientSharedImage> GraphicsDelegateAndroid::GetTexture() {
  BufferSlot& slot = buffers_[current_buffer_index_];
  if (!slot.shared_buffer) {
    return nullptr;
  }
  // Return `unowned_shared_image` (which has no MappableBuffer) when this
  // slot's AHardwareBuffer has already been exported so
  // `ClientSharedImage::Export()` does not clone the AHardwareBuffer FD on
  // every frame, and `OpenXrGraphicsBindingOpenGLES` reuses its cached
  // EGLImage/texture for the slot index encoded in `GetSyncToken()`.
  if (!slot.needs_handle_export) {
    return slot.unowned_shared_image;
  }
  slot.needs_handle_export = false;
  return slot.shared_buffer->shared_image;
}

gpu::SyncToken GraphicsDelegateAndroid::GetSyncToken() {
  // Encode the active double-buffer slot index in release_count (1-based) so
  // OpenXrGraphicsBindingOpenGLES::SetOverlayTexture() can switch between its
  // cached textures without needing a new GpuMemoryBufferHandle IPC every
  // frame.
  gpu::SyncToken token(gpu::CommandBufferNamespace::GPU_IO,
                       gpu::CommandBufferId(),
                       static_cast<uint64_t>(current_buffer_index_ + 1));
  token.SetVerifyFlush();
  return token;
}

bool GraphicsDelegateAndroid::EnsureAndBindMemoryBuffer() {
  const gfx::Size buffer_size = GetTextureSize();
  if (last_size_ != buffer_size) {
    ResetMemoryBuffer();
    last_size_ = buffer_size;
  }

  BufferSlot& slot = buffers_[current_buffer_index_];
  if (slot.shared_buffer && slot.shared_buffer->shared_image &&
      slot.shared_buffer->scoped_ahb_handle.is_valid()) {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, slot.draw_frame_buffer);
    return true;
  }

  if (!slot.shared_buffer) {
    slot.shared_buffer = std::make_unique<device::WebXrSharedBuffer>();
    slot.shared_buffer->local_texture.target = GL_TEXTURE_2D;
    glGenTextures(1, &slot.shared_buffer->local_texture.id);
  }

  if (slot.shared_buffer->shared_image) {
    slot.unowned_shared_image.reset();
    mailbox_bridge_->DestroySharedImage(
        slot.shared_buffer->sync_token,
        std::move(slot.shared_buffer->shared_image));
  }

  // Remove reference to previous image (if any).
  slot.shared_buffer->local_eglimage.reset();

  static constexpr viz::SharedImageFormat kFormat =
      viz::SinglePlaneFormat::kRGBA_8888;
  static constexpr gfx::BufferUsage kUsage = gfx::BufferUsage::SCANOUT;
  static constexpr gpu::SharedImageUsageSet kSharedImageUsage =
      gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;

  base::android::ScopedHardwareBufferHandle scoped_ahb_handle =
      gpu::CreateScopedHardwareBufferHandle(buffer_size, kFormat, kUsage);
  if (!scoped_ahb_handle.is_valid()) {
    return false;
  }

  // Create an EGLImage for the buffer.
  gl::ScopedEGLImage egl_image =
      gpu::CreateEGLImageFromAHardwareBuffer(scoped_ahb_handle.get());
  if (!egl_image.is_valid()) {
    return false;
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  gmb_handle.android_hardware_buffer = scoped_ahb_handle.Clone();

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      mailbox_bridge_->CreateSharedImage(
          std::move(gmb_handle), kFormat, buffer_size, gfx::ColorSpace(),
          kTopLeft_GrSurfaceOrigin, kSharedImageUsage,
          slot.shared_buffer->sync_token);
  if (!shared_image) {
    return false;
  }

  const GLenum target = slot.shared_buffer->local_texture.target;
  glBindTexture(target, slot.shared_buffer->local_texture.id);
  glEGLImageTargetTexture2DOES(target, egl_image.get());
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(target, 0);

  slot.shared_buffer->scoped_ahb_handle = std::move(scoped_ahb_handle);
  slot.shared_buffer->local_eglimage = std::move(egl_image);
  slot.shared_buffer->shared_image = std::move(shared_image);
  slot.unowned_shared_image = slot.shared_buffer->shared_image->MakeUnowned();
  slot.needs_handle_export = true;

  if (slot.draw_frame_buffer == 0) {
    glGenFramebuffersEXT(1, &slot.draw_frame_buffer);
  }
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, slot.draw_frame_buffer);
  glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            slot.shared_buffer->local_texture.id, 0);
  ClearBufferToBlack();

  return true;
}

void GraphicsDelegateAndroid::ResetMemoryBuffer() {
  last_size_ = gfx::Size();
  for (BufferSlot& slot : buffers_) {
    slot.needs_handle_export = false;
    slot.unowned_shared_image.reset();
    if (slot.draw_frame_buffer != 0) {
      glDeleteFramebuffersEXT(1, &slot.draw_frame_buffer);
      slot.draw_frame_buffer = 0;
    }
    if (slot.shared_buffer) {
      if (slot.shared_buffer->shared_image && mailbox_bridge_) {
        mailbox_bridge_->DestroySharedImage(
            slot.shared_buffer->sync_token,
            std::move(slot.shared_buffer->shared_image));
      }
      slot.shared_buffer->local_eglimage.reset();
      slot.shared_buffer->scoped_ahb_handle.reset();
    }
  }
}

void GraphicsDelegateAndroid::ClearBufferToBlack() {
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
}

}  // namespace vr
