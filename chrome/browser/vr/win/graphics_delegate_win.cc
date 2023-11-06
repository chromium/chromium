// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/graphics_delegate_win.h"

#include "base/numerics/math_constants.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/gpu_stream_constants.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

namespace vr {

GraphicsDelegateWin::GraphicsDelegateWin() {
  gpu::GpuChannelEstablishFactory* factory =
      content::GetGpuChannelEstablishFactory();
  gpu_channel_host_ = factory->EstablishGpuChannelSync();

  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;

  context_provider_ = base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      gpu_channel_host_, content::kGpuStreamIdDefault,
      content::kGpuStreamPriorityUI, gpu::kNullSurfaceHandle,
      GURL(std::string("chrome://gpu/VrUiWin")), false /* automatic flushes */,
      false /* support locking */, false /* support grcontext */,
      gpu::SharedMemoryLimits::ForMailboxContext(), attributes,
      viz::command_buffer_metrics::ContextType::XR_COMPOSITING);

  if (context_provider_->BindToCurrentSequence() ==
      gpu::ContextResult::kSuccess) {
    gl_ = context_provider_->ContextGL();
    sii_ = context_provider_->SharedImageInterface();
  }
}

GraphicsDelegateWin::~GraphicsDelegateWin() = default;

bool GraphicsDelegateWin::BindContext() {
  if (!gl_)
    return false;

  gles2::SetGLContext(gl_);
  return true;
}

void GraphicsDelegateWin::ClearContext() {
  gles2::SetGLContext(nullptr);
}

bool GraphicsDelegateWin::PreRender() {
  if (!gl_)
    return false;

  BindContext();

  // Create a memory buffer and a shared image referencing that memory buffer.
  if (!EnsureMemoryBuffer()) {
    return false;
  }

  // Create a texture id and associate it with shared image.
  dest_texture_id_ =
      gl_->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox_.name);
  gl_->BeginSharedImageAccessDirectCHROMIUM(
      dest_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  gl_->BindTexture(GL_TEXTURE_2D, dest_texture_id_);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Bind our image/texture/memory buffer as the draw framebuffer.
  gl_->GenFramebuffers(1, &draw_frame_buffer_);
  gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer_);
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, dest_texture_id_, 0);

  if (gl_->GetError() != GL_NO_ERROR) {
    // Clear any remaining GL errors.
    while (gl_->GetError() != GL_NO_ERROR) {
    }
    return false;
  }

  return true;
}

void GraphicsDelegateWin::PostRender() {
  // Unbind the drawing buffer.
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl_->DeleteFramebuffers(1, &draw_frame_buffer_);

  gl_->EndSharedImageAccessDirectCHROMIUM(dest_texture_id_);
  gl_->DeleteTextures(1, &dest_texture_id_);
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  dest_texture_id_ = 0;
  draw_frame_buffer_ = 0;

  // Generate a SyncToken after GPU is done accessing the texture.
  gl_->GenSyncTokenCHROMIUM(access_done_sync_token_.GetData());

  // Flush.
  gl_->ShallowFlushCHROMIUM();
  ClearContext();
}

mojo::PlatformHandle GraphicsDelegateWin::GetTexture() {
  if (buffer_handle_.is_null()) {
    return {};
  }

  gfx::GpuMemoryBufferHandle gpu_handle = buffer_handle_.Clone();
  return mojo::PlatformHandle(std::move(gpu_handle.dxgi_handle));
}

const gpu::SyncToken& GraphicsDelegateWin::GetSyncToken() {
  return access_done_sync_token_;
}

bool GraphicsDelegateWin::EnsureMemoryBuffer() {
  gfx::Size buffer_size = GetTextureSize();
  if (!buffer_handle_.is_null() && last_size_ == buffer_size) {
    return true;
  }

  if (!mailbox_.IsZero()) {
    sii_->DestroySharedImage(access_done_sync_token_, mailbox_);
    mailbox_.SetZero();
    access_done_sync_token_.Clear();
  }

  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;

  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_allow_sync_call;

    gpu_channel_host_->CreateGpuMemoryBuffer(
        buffer_size, format, gfx::BufferUsage::SCANOUT, &buffer_handle_);
  }

  if (buffer_handle_.is_null()) {
    return false;
  }

  last_size_ = buffer_size;

  auto client_shared_image = sii_->CreateSharedImage(
      format, buffer_size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_GLES2 |
          gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT,
      "VRGraphicsDelegate", buffer_handle_.Clone());
  CHECK(client_shared_image);
  mailbox_ = client_shared_image->mailbox();

  gl_->WaitSyncTokenCHROMIUM(sii_->GenUnverifiedSyncToken().GetConstData());
  return true;
}

void GraphicsDelegateWin::ResetMemoryBuffer() {
  // Stop using a memory buffer if we had an error submitting with it.
  buffer_handle_ = gfx::GpuMemoryBufferHandle();
}

void GraphicsDelegateWin::ClearBufferToBlack() {
  gl_->ClearColor(0, 0, 0, 0);
  gl_->Clear(GL_COLOR_BUFFER_BIT);
}

}  // namespace vr
