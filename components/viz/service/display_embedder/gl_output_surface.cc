// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/math_util.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/renderer_utils.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/config/gpu_feature_info.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {

GLOutputSurface::GLOutputSurface(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle)
    : OutputSurface(context_provider),
      viz_context_provider_(context_provider),
      surface_handle_(surface_handle),
      use_gpu_fence_(
          context_provider->ContextCapabilities().chromium_gpu_fence &&
          context_provider->ContextCapabilities()
              .use_gpu_fences_for_overlay_planes) {
  const auto& context_capabilities = context_provider->ContextCapabilities();
  capabilities_.output_surface_origin = context_capabilities.surface_origin;
  capabilities_.supports_stencil = context_capabilities.num_stencil_bits > 0;
  // Since one of the buffers is used by the surface for presentation, there can
  // be at most |num_surface_buffers - 1| pending buffers that the compositor
  // can use.
  capabilities_.max_frames_pending =
      context_capabilities.num_surface_buffers - 1;
  capabilities_.supports_gpu_vsync = context_capabilities.gpu_vsync;
  capabilities_.supports_dc_layers = context_capabilities.dc_layers;
  capabilities_.supports_surfaceless = context_capabilities.surfaceless;
  capabilities_.android_surface_control_feature_enabled =
      context_provider->GetGpuFeatureInfo()
          .status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] ==
      gpu::kGpuFeatureStatusEnabled;
  capabilities_.max_render_target_size = context_capabilities.max_texture_size;
}

GLOutputSurface::~GLOutputSurface() {
  viz_context_provider_->SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback());
  viz_context_provider_->SetGpuVSyncCallback(GpuVSyncCallback());
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
  DCHECK(capabilities_.supports_dc_layers);

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

void GLOutputSurface::SetEnableDCLayers(bool enable) {
  DCHECK(capabilities_.supports_dc_layers);
  context_provider()->ContextGL()->SetEnableDCLayersCHROMIUM(enable);
}

void GLOutputSurface::Reshape(const gfx::Size& size,
                              float device_scale_factor,
                              const gfx::ColorSpace& color_space,
                              gfx::BufferFormat format,
                              bool use_stencil) {
  size_ = size;
  has_set_draw_rectangle_since_last_resize_ = false;
  set_draw_rectangle_for_frame_ = false;
  context_provider()->ContextGL()->ResizeCHROMIUM(
      size.width(), size.height(), device_scale_factor,
      color_space.AsGLColorSpace(), gfx::AlphaBitsForBufferFormat(format));
}

void GLOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(context_provider_);

  uint32_t flags = 0;
  if (wants_vsync_parameter_updates_)
    flags |= gpu::SwapBuffersFlags::kVSyncParams;

  // The |swap_size| here should always be in the UI's logical screen space
  // since it is forwarded to the client code which is unaware of the display
  // transform optimization.
  gfx::Size swap_size = ApplyDisplayInverse(gfx::Rect(size_)).size();
  auto swap_callback = base::BindOnce(
      &GLOutputSurface::OnGpuSwapBuffersCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(frame.latency_info),
      frame.top_controls_visible_height_changed, swap_size);
  gpu::ContextSupport::PresentationCallback presentation_callback;
  presentation_callback = base::BindOnce(&GLOutputSurface::OnPresentation,
                                         weak_ptr_factory_.GetWeakPtr());

  set_draw_rectangle_for_frame_ = false;
  if (frame.sub_buffer_rect) {
    HandlePartialSwap(*frame.sub_buffer_rect, flags, std::move(swap_callback),
                      std::move(presentation_callback));
  } else if (!frame.content_bounds.empty()) {
    context_provider_->ContextSupport()->SwapWithBounds(
        frame.content_bounds, flags, std::move(swap_callback),
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

bool GLOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned GLOutputSurface::GetOverlayTextureId() const {
  return 0;
}

bool GLOutputSurface::HasExternalStencilTest() const {
  return false;
}

void GLOutputSurface::ApplyExternalStencil() {}

void GLOutputSurface::DidReceiveSwapBuffersAck(
    const gfx::SwapResponse& response) {
  client_->DidReceiveSwapBuffersAck(response.timings);
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
    bool top_controls_visible_height_changed,
    const gfx::Size& pixel_size,
    const gpu::SwapBuffersCompleteParams& params) {
  if (!params.texture_in_use_responses.empty())
    client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  DidReceiveSwapBuffersAck(params.swap_response);

  UpdateLatencyInfoOnSwap(params.swap_response, &latency_info);
  latency_tracker_.OnGpuSwapBuffersCompleted(
      latency_info, top_controls_visible_height_changed);

  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
}

void GLOutputSurface::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  client_->DidReceivePresentationFeedback(feedback);
}

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

void GLOutputSurface::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  wants_vsync_parameter_updates_ = !callback.is_null();
  viz_context_provider_->SetUpdateVSyncParametersCallback(std::move(callback));
}

void GLOutputSurface::SetGpuVSyncCallback(GpuVSyncCallback callback) {
  DCHECK(capabilities_.supports_gpu_vsync);
  viz_context_provider_->SetGpuVSyncCallback(std::move(callback));
}

void GLOutputSurface::SetGpuVSyncEnabled(bool enabled) {
  DCHECK(capabilities_.supports_gpu_vsync);
  viz_context_provider_->SetGpuVSyncEnabled(enabled);
}

gfx::OverlayTransform GLOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

gfx::Rect GLOutputSurface::ApplyDisplayInverse(const gfx::Rect& input) {
  gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
      gfx::InvertOverlayTransform(GetDisplayTransform()), gfx::SizeF(size_));
  return cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
      display_inverse, input);
}

base::ScopedClosureRunner GLOutputSurface::GetCacheBackBufferCb() {
  return viz_context_provider_->GetCacheBackBufferCb();
}

gpu::SurfaceHandle GLOutputSurface::GetSurfaceHandle() const {
  return surface_handle_;
}

void GLOutputSurface::SetFrameRate(float frame_rate) {
  viz_context_provider_->ContextSupport()->SetFrameRate(frame_rate);
}

void GLOutputSurface::SetNeedsMeasureNextDrawLatency() {
  viz_context_provider_->SetNeedsMeasureNextDrawLatency();
}

}  // namespace viz
