// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/graphics_delegate_win.h"

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

GraphicsDelegateWin::GraphicsDelegateWin() = default;
GraphicsDelegateWin::~GraphicsDelegateWin() = default;

void GraphicsDelegateWin::Initialize(base::OnceClosure on_initialized) {
  gpu::GpuChannelEstablishFactory* factory =
      content::GetGpuChannelEstablishFactory();
  gpu_channel_host_ = factory->EstablishGpuChannelSync();

  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;

  context_provider_ = base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      gpu_channel_host_, content::kGpuStreamIdDefault,
      content::kGpuStreamPriorityUI, gpu::kNullSurfaceHandle,
      GURL(std::string("chrome://gpu/VrUiWin")), false /* automatic flushes */,
      false /* support locking */, gpu::SharedMemoryLimits::ForMailboxContext(),
      attributes, viz::command_buffer_metrics::ContextType::XR_COMPOSITING);

  if (context_provider_->BindToCurrentSequence() ==
      gpu::ContextResult::kSuccess) {
    gl_ = context_provider_->ContextGL();
    sii_ = context_provider_->SharedImageInterface();
  }

  std::move(on_initialized).Run();
}

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
  shared_image_texture_ = client_shared_image_->CreateGLTexture(gl_);
  scoped_shared_image_access_ =
      shared_image_texture_->BeginAccess(gpu::SyncToken(), /*readonly=*/false);

  gl_->BindTexture(GL_TEXTURE_2D, scoped_shared_image_access_->texture_id());
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Bind our image/texture/memory buffer as the draw framebuffer.
  gl_->GenFramebuffers(1, &draw_frame_buffer_);
  gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer_);
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            scoped_shared_image_access_->texture_id(), 0);

  if (gl_->GetError() != GL_NO_ERROR) {
    gpu::SharedImageTexture::ScopedAccess::EndAccess(
        std::move(scoped_shared_image_access_));
    shared_image_texture_.reset();
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

  // Generate a SyncToken after GPU is done accessing the texture.
  access_done_sync_token_ = gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(scoped_shared_image_access_));
  shared_image_texture_.reset();
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  draw_frame_buffer_ = 0;

  // Flush.
  gl_->ShallowFlushCHROMIUM();
  ClearContext();
}

gfx::GpuMemoryBufferHandle GraphicsDelegateWin::GetTexture() {
  if (!client_shared_image_) {
    return gfx::GpuMemoryBufferHandle();
  }

  return client_shared_image_->CloneGpuMemoryBufferHandle();
}

gpu::SyncToken GraphicsDelegateWin::GetSyncToken() {
  return access_done_sync_token_;
}

bool GraphicsDelegateWin::EnsureMemoryBuffer() {
  gfx::Size buffer_size = GetTextureSize();
  if (client_shared_image_ && last_size_ == buffer_size) {
    return true;
  }

  // Destroy any existing SharedImage as its size is not correct.
  ResetMemoryBuffer();

  last_size_ = buffer_size;

  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;

  // These SharedImages will be written to via GLES2 to render the scene,
  // followed by having their underlying GMBHandle sent off to be displayed in
  // an overlay.
  client_shared_image_ = sii_->CreateSharedImage(
      {format, buffer_size, gfx::ColorSpace(),
       gpu::SHARED_IMAGE_USAGE_GLES2_WRITE, "VRGraphicsDelegate"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT);
  if (!client_shared_image_) {
    return false;
  }

  gl_->WaitSyncTokenCHROMIUM(sii_->GenUnverifiedSyncToken().GetConstData());
  return true;
}

void GraphicsDelegateWin::ResetMemoryBuffer() {
  if (client_shared_image_) {
    sii_->DestroySharedImage(access_done_sync_token_,
                             std::move(client_shared_image_));
  }
  access_done_sync_token_.Clear();
}

void GraphicsDelegateWin::ClearBufferToBlack() {
  gl_->ClearColor(0, 0, 0, 0);
  gl_->Clear(GL_COLOR_BUFFER_BIT);
}

}  // namespace vr
