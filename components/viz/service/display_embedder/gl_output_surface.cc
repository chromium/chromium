// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "ui/gl/gl_utils.h"

namespace viz {

GLOutputSurface::GLOutputSurface(
    scoped_refptr<VizProcessContextProvider> context_provider,
    SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : OutputSurface(context_provider),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      use_gpu_fence_(
          context_provider->ContextCapabilities().chromium_gpu_fence &&
          context_provider->ContextCapabilities()
              .use_gpu_fences_for_overlay_planes),
      weak_ptr_factory_(this) {
  capabilities_.flipped_output_surface =
      context_provider->ContextCapabilities().flips_vertically;
  capabilities_.supports_stencil =
      context_provider->ContextCapabilities().num_stencil_bits > 0;
  // Since one of the buffers is used by the surface for presentation, there can
  // be at most |num_surface_buffers - 1| pending buffers that the compositor
  // can use.
  capabilities_.max_frames_pending =
      context_provider->ContextCapabilities().num_surface_buffers - 1;
  context_provider->SetUpdateVSyncParametersCallback(
      base::BindRepeating(&GLOutputSurface::OnVSyncParametersUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

GLOutputSurface::~GLOutputSurface() {
  if (gpu_fence_id_ > 0)
    context_provider()->ContextGL()->DestroyGpuFenceCHROMIUM(gpu_fence_id_);
}

void GLOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void GLOutputSurface::EnsureBackbuffer() {}

void GLOutputSurface::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void GLOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLOutputSurface::SetDrawRectangle(const gfx::Rect& rect) {
  if (set_draw_rectangle_for_frame_)
    return;
  DCHECK(gfx::Rect(size_).Contains(rect));
  DCHECK(has_set_draw_rectangle_since_last_resize_ ||
         (gfx::Rect(size_) == rect));
  set_draw_rectangle_for_frame_ = true;
  has_set_draw_rectangle_since_last_resize_ = true;
  context_provider()->ContextGL()->SetDrawRectangleCHROMIUM(
      rect.x(), rect.y(), rect.width(), rect.height());
}

void GLOutputSurface::Reshape(const gfx::Size& size,
                              float device_scale_factor,
                              const gfx::ColorSpace& color_space,
                              bool has_alpha,
                              bool use_stencil) {
  size_ = size;
  has_set_draw_rectangle_since_last_resize_ = false;
  context_provider()->ContextGL()->ResizeCHROMIUM(
      size.width(), size.height(), device_scale_factor,
      gl::GetGLColorSpace(color_space), has_alpha);
}

void GLOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(context_provider_);

  uint32_t flags = 0;
  if (synthetic_begin_frame_source_)
    flags |= gpu::SwapBuffersFlags::kVSyncParams;

  auto swap_callback = base::BindOnce(
      &GLOutputSurface::OnGpuSwapBuffersCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(frame.latency_info), size_);
  gpu::ContextSupport::PresentationCallback presentation_callback;
  if (frame.need_presentation_feedback) {
    flags |= gpu::SwapBuffersFlags::kPresentationFeedback;
    presentation_callback = base::BindOnce(&GLOutputSurface::OnPresentation,
                                           weak_ptr_factory_.GetWeakPtr());
  }

  set_draw_rectangle_for_frame_ = false;
  if (frame.sub_buffer_rect) {
    HandlePartialSwap(*frame.sub_buffer_rect, flags, std::move(swap_callback),
                      std::move(presentation_callback));
  } else {
    context_provider_->ContextSupport()->Swap(flags, std::move(swap_callback),
                                              std::move(presentation_callback));
  }
}

uint32_t GLOutputSurface::GetFramebufferCopyTextureFormat() {
  auto* gl = static_cast<VizProcessContextProvider*>(context_provider());
  return gl->GetCopyTextureInternalFormat();
}

OverlayCandidateValidator* GLOutputSurface::GetOverlayCandidateValidator()
    const {
  return nullptr;
}

bool GLOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned GLOutputSurface::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat GLOutputSurface::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

bool GLOutputSurface::HasExternalStencilTest() const {
  return false;
}

void GLOutputSurface::ApplyExternalStencil() {}

void GLOutputSurface::DidReceiveSwapBuffersAck(gfx::SwapResult result) {
  client_->DidReceiveSwapBuffersAck();
}

void GLOutputSurface::HandlePartialSwap(
    const gfx::Rect& sub_buffer_rect,
    uint32_t flags,
    gpu::ContextSupport::SwapCompletedCallback swap_callback,
    gpu::ContextSupport::PresentationCallback presentation_callback) {
  context_provider_->ContextSupport()->PartialSwapBuffers(
      sub_buffer_rect, flags, std::move(swap_callback),
      std::move(presentation_callback));
}

void GLOutputSurface::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info,
    const gfx::Size& pixel_size,
    const gpu::SwapBuffersCompleteParams& params) {
  if (!params.texture_in_use_responses.empty())
    client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  DidReceiveSwapBuffersAck(params.swap_response.result);

  UpdateLatencyInfoOnSwap(params.swap_response, &latency_info);
  latency_tracker_.OnGpuSwapBuffersCompleted(latency_info);
  client_->DidFinishLatencyInfo(latency_info);

  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
}

void GLOutputSurface::OnVSyncParametersUpdated(base::TimeTicks timebase,
                                               base::TimeDelta interval) {
  if (synthetic_begin_frame_source_) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    synthetic_begin_frame_source_->OnUpdateVSyncParameters(
        timebase,
        interval.is_zero() ? BeginFrameArgs::DefaultInterval() : interval);
  }
}

void GLOutputSurface::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  client_->DidReceivePresentationFeedback(feedback);
}

#if BUILDFLAG(ENABLE_VULKAN)
gpu::VulkanSurface* GLOutputSurface::GetVulkanSurface() {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

unsigned GLOutputSurface::UpdateGpuFence() {
  if (!use_gpu_fence_)
    return 0;

  if (gpu_fence_id_ > 0)
    context_provider()->ContextGL()->DestroyGpuFenceCHROMIUM(gpu_fence_id_);

  gpu_fence_id_ = context_provider()->ContextGL()->CreateGpuFenceCHROMIUM();

  return gpu_fence_id_;
}

void GLOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}

}  // namespace viz
