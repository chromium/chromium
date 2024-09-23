// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/graphics_delegate_android.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/ipc/common/android/android_hardware_buffer_utils.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

GraphicsDelegateAndroid::GraphicsDelegateAndroid() = default;
GraphicsDelegateAndroid::~GraphicsDelegateAndroid() = default;

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
    base::OnceClosure on_inititalized) {
  DCHECK(mailbox_bridge_->IsConnected());
  std::move(on_inititalized).Run();
}

bool GraphicsDelegateAndroid::BindContext() {
  return true;
}

void GraphicsDelegateAndroid::ClearContext() {}

bool GraphicsDelegateAndroid::PreRender() {
  BindContext();

  // Create a memory buffer and a shared image referencing that memory buffer.
  if (!EnsureMemoryBuffer()) {
    DLOG(ERROR) << __func__ << " Failed to ensure memory buffer";
    return false;
  }
  DVLOG(3) << __func__ << " mailbox: "
           << shared_buffer_->shared_image->mailbox().ToDebugString()
           << " sync_token: " << shared_buffer_->sync_token.ToDebugString();

  GLenum target = shared_buffer_->local_texture.target;
  glBindTexture(target, shared_buffer_->local_texture.id);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Bind our image/texture/memory buffer as the draw framebuffer.
  glGenFramebuffersEXT(1, &draw_frame_buffer_);
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, draw_frame_buffer_);
  glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            shared_buffer_->local_texture.id, 0);

  return true;
}

void GraphicsDelegateAndroid::PostRender() {
  // Unbind the drawing buffer.
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffersEXT(1, &draw_frame_buffer_);

  glBindTexture(GL_TEXTURE_2D, 0);
  draw_frame_buffer_ = 0;

  // Generate a SyncToken after GPU is done accessing the texture.
  mailbox_bridge_->GenSyncToken(&shared_buffer_->sync_token);

  ClearContext();
  glFlush();
}

gfx::GpuMemoryBufferHandle GraphicsDelegateAndroid::GetTexture() {
  if (!shared_buffer_) {
    return gfx::GpuMemoryBufferHandle();
  }
  return gfx::GpuMemoryBufferHandle(shared_buffer_->scoped_ahb_handle.Clone());
}

gpu::SyncToken GraphicsDelegateAndroid::GetSyncToken() {
  if (!shared_buffer_) {
    return gpu::SyncToken();
  }

  return shared_buffer_->sync_token;
}

bool GraphicsDelegateAndroid::EnsureMemoryBuffer() {
  // The code here is very similar to that used in both XrImageTransportBase
  // and OpenXrGraphicsDelegateOpenGLES's ResizeShardBuffer methods. However,
  // they all have subtly different uses.
  // TODO(https://crbug.com/40909689): Consolidate this usage.
  gfx::Size buffer_size = GetTextureSize();
  if (shared_buffer_ && shared_buffer_->size == buffer_size) {
    return true;
  }

  if (!shared_buffer_) {
    shared_buffer_ = std::make_unique<device::WebXrSharedBuffer>();
    shared_buffer_->local_texture.target =
        device::XrImageTransportBase::SharedBufferTextureTarget();
    glGenTextures(1, &shared_buffer_->local_texture.id);
  }

  if (shared_buffer_->shared_image) {
    mailbox_bridge_->DestroySharedImage(
        shared_buffer_->sync_token, std::move(shared_buffer_->shared_image));
  }

  // Remove reference to previous image (if any).
  shared_buffer_->local_eglimage.reset();

  static constexpr gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  static constexpr gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;

  shared_buffer_->scoped_ahb_handle =
      gpu::CreateScopedHardwareBufferHandle(buffer_size, format, usage);

  // Create a GMB Handle from AHardwareBuffer handle.
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  // GpuMemoryBufferId is not used in this case and hence hardcoding it to 1
  // here.
  gmb_handle.id = gfx::GpuMemoryBufferId(1);
  gmb_handle.android_hardware_buffer =
      shared_buffer_->scoped_ahb_handle.Clone();

  shared_buffer_->shared_image = mailbox_bridge_->CreateSharedImage(
      std::move(gmb_handle), format, buffer_size, gfx::ColorSpace(),
      shared_image_usage, shared_buffer_->sync_token);
  DVLOG(2) << ": CreateSharedImage, mailbox="
           << shared_buffer_->shared_image->mailbox().ToDebugString()
           << ", SyncToken=" << shared_buffer_->sync_token.ToDebugString()
           << ", size=" << buffer_size.ToString();

  // Create an EGLImage for the buffer.
  auto egl_image = gpu::CreateEGLImageFromAHardwareBuffer(
      shared_buffer_->scoped_ahb_handle.get());
  if (!egl_image.is_valid()) {
    return false;
  }

  glBindTexture(shared_buffer_->local_texture.target,
                shared_buffer_->local_texture.id);
  glEGLImageTargetTexture2DOES(shared_buffer_->local_texture.target,
                               egl_image.get());
  shared_buffer_->local_eglimage = std::move(egl_image);

  // Save size to avoid resize next time.
  DVLOG(1) << __func__ << ": resized to " << buffer_size.width() << "x"
           << buffer_size.height();
  shared_buffer_->size = buffer_size;
  return true;
}

void GraphicsDelegateAndroid::ResetMemoryBuffer() {
  // Stop using a memory buffer if we had an error submitting with it.
  if (shared_buffer_ && shared_buffer_->shared_image) {
    DCHECK(mailbox_bridge_);
    DVLOG(2) << ": DestroySharedImage, mailbox="
             << shared_buffer_->shared_image->mailbox().ToDebugString();
    mailbox_bridge_->DestroySharedImage(
        shared_buffer_->sync_token, std::move(shared_buffer_->shared_image));
    shared_buffer_->size = {0, 0};
  }
}

void GraphicsDelegateAndroid::ClearBufferToBlack() {
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
}

}  // namespace vr
