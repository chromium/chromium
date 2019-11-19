// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_offscreen.h"

#include <stdint.h>

#include "base/bind.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gl/gl_utils.h"

namespace viz {
namespace {

constexpr ResourceFormat kFboTextureFormat = RGBA_8888;

}  // namespace

GLOutputSurfaceOffscreen::GLOutputSurfaceOffscreen(
    scoped_refptr<VizProcessContextProvider> context_provider)
    : GLOutputSurface(context_provider, gpu::kNullSurfaceHandle) {}

GLOutputSurfaceOffscreen::~GLOutputSurfaceOffscreen() {
  DiscardBackbuffer();
}

void GLOutputSurfaceOffscreen::EnsureBackbuffer() {
  if (size_.IsEmpty())
    return;

  if (!texture_id_) {
    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

    const int max_texture_size =
        context_provider_->ContextCapabilities().max_texture_size;
    gfx::Size texture_size(std::min(size_.width(), max_texture_size),
                           std::min(size_.height(), max_texture_size));

    const uint32_t flags = gpu::SHARED_IMAGE_USAGE_GLES2 |
                           gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                           gpu::SHARED_IMAGE_USAGE_DISPLAY;
    mailbox_ = sii->CreateSharedImage(kFboTextureFormat, texture_size,
                                      color_space_, flags);

    // Ensure mailbox is valid before using it.
    gl->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

    texture_id_ = gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox_.name);

    gl->GenFramebuffers(1, &fbo_);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture_id_, 0);
  }
}

void GLOutputSurfaceOffscreen::DiscardBackbuffer() {
  if (fbo_) {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->DeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }

  if (texture_id_) {
    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    sii->DestroySharedImage(gpu::SyncToken(), mailbox_);
    mailbox_.SetZero();
    texture_id_ = 0;
  }
}

void GLOutputSurfaceOffscreen::BindFramebuffer() {
  if (!texture_id_) {
    EnsureBackbuffer();
  } else {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }
}

void GLOutputSurfaceOffscreen::Reshape(const gfx::Size& size,
                                       float scale_factor,
                                       const gfx::ColorSpace& color_space,
                                       bool alpha,
                                       bool stencil) {
  size_ = size;
  color_space_ = color_space;
  DiscardBackbuffer();
  EnsureBackbuffer();
}

void GLOutputSurfaceOffscreen::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(frame.size == size_);

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token,
      base::BindOnce(&GLOutputSurfaceOffscreen::OnSwapBuffersComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(frame.latency_info)));
}

void GLOutputSurfaceOffscreen::OnSwapBuffersComplete(
    std::vector<ui::LatencyInfo> latency_info) {
  latency_tracker()->OnGpuSwapBuffersCompleted(latency_info);
  // Swap timings are not available since for offscreen there is no Swap, just a
  // SignalSyncToken. We use base::TimeTicks::Now() as an overestimate.
  auto now = base::TimeTicks::Now();
  client()->DidReceiveSwapBuffersAck({.swap_start = now});
  client()->DidReceivePresentationFeedback(gfx::PresentationFeedback(
      now, base::TimeDelta::FromMilliseconds(16), /*flags=*/0));

  if (needs_swap_size_notifications())
    client()->DidSwapWithSize(size_);
}

}  // namespace viz
