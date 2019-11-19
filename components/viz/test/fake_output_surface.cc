// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_output_surface.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/color_space_utils.h"

namespace viz {

FakeOutputSurface::FakeOutputSurface(
    scoped_refptr<ContextProvider> context_provider)
    : OutputSurface(std::move(context_provider)) {
  DCHECK(OutputSurface::context_provider());
}

FakeOutputSurface::FakeOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(std::move(software_device)) {
  DCHECK(OutputSurface::software_device());
}

FakeOutputSurface::~FakeOutputSurface() = default;

void FakeOutputSurface::Reshape(const gfx::Size& size,
                                float device_scale_factor,
                                const gfx::ColorSpace& color_space,
                                bool has_alpha,
                                bool use_stencil) {
  if (context_provider()) {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor,
        gl::ColorSpaceUtils::GetGLColorSpace(color_space), has_alpha);
  } else {
    software_device()->Resize(size, device_scale_factor);
  }
  last_reshape_color_space_ = color_space;
}

void FakeOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  last_sent_frame_ = std::make_unique<OutputSurfaceFrame>(std::move(frame));
  ++num_sent_frames_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOutputSurface::SwapBuffersAck,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeOutputSurface::SwapBuffersAck() {
  base::TimeTicks now = base::TimeTicks::Now();
  client_->DidReceiveSwapBuffersAck({now, now});
  client_->DidReceivePresentationFeedback({now, base::TimeDelta(), 0});
}

void FakeOutputSurface::BindFramebuffer() {
  context_provider_->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

void FakeOutputSurface::SetDrawRectangle(const gfx::Rect& rect) {
  last_set_draw_rectangle_ = rect;
}

uint32_t FakeOutputSurface::GetFramebufferCopyTextureFormat() {
  if (framebuffer_)
    return framebuffer_format_;
  else
    return GL_RGB;
}

void FakeOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

bool FakeOutputSurface::HasExternalStencilTest() const {
  return has_external_stencil_test_;
}

gfx::BufferFormat FakeOutputSurface::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

bool FakeOutputSurface::IsDisplayedAsOverlayPlane() const {
  return overlay_texture_id_ != 0;
}

unsigned FakeOutputSurface::GetOverlayTextureId() const {
  return overlay_texture_id_;
}

unsigned FakeOutputSurface::UpdateGpuFence() {
  return gpu_fence_id_;
}

void FakeOutputSurface::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {}

gfx::OverlayTransform FakeOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void FakeOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {}
#endif

}  // namespace viz
