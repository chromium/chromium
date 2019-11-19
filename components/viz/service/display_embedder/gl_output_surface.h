// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_H_

#include <memory>

#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "ui/latency/latency_tracker.h"

namespace viz {

// An OutputSurface implementation that directly draws and
// swaps to an actual GL surface.
class GLOutputSurface : public OutputSurface {
 public:
  GLOutputSurface(scoped_refptr<VizProcessContextProvider> context_provider,
                  gpu::SurfaceHandle surface_handle);
  ~GLOutputSurface() override;

  // OutputSurface implementation
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  unsigned UpdateGpuFence() override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetGpuVSyncCallback(GpuVSyncCallback callback) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;
  base::ScopedClosureRunner GetCacheBackBufferCb() override;

  gpu::SurfaceHandle GetSurfaceHandle() const override;

 protected:
  OutputSurfaceClient* client() const { return client_; }
  ui::LatencyTracker* latency_tracker() { return &latency_tracker_; }
  bool needs_swap_size_notifications() {
    return needs_swap_size_notifications_;
  }

  // Called when a swap completion is signaled from ImageTransportSurface.
  virtual void DidReceiveSwapBuffersAck(const gfx::SwapResponse& response);

  // Called in SwapBuffers() when a swap is determined to be partial. Subclasses
  // might override this method because different platforms handle partial swaps
  // differently.
  virtual void HandlePartialSwap(
      const gfx::Rect& sub_buffer_rect,
      uint32_t flags,
      gpu::ContextSupport::SwapCompletedCallback swap_callback,
      gpu::ContextSupport::PresentationCallback presentation_callback);

 private:
  // Called when a swap completion is signaled from ImageTransportSurface.
  void OnGpuSwapBuffersCompleted(std::vector<ui::LatencyInfo> latency_info,
                                 const gfx::Size& pixel_size,
                                 const gpu::SwapBuffersCompleteParams& params);
  void OnPresentation(const gfx::PresentationFeedback& feedback);
  void OnGpuVSync(base::TimeTicks vsync_time, base::TimeDelta vsync_interval);
  gfx::Rect ApplyDisplayInverse(const gfx::Rect& input);

  scoped_refptr<VizProcessContextProvider> viz_context_provider_;
  OutputSurfaceClient* client_ = nullptr;
  bool wants_vsync_parameter_updates_ = false;
  ui::LatencyTracker latency_tracker_;

  const gpu::SurfaceHandle surface_handle_;

  bool set_draw_rectangle_for_frame_ = false;
  // True if the draw rectangle has been set at all since the last resize.
  bool has_set_draw_rectangle_since_last_resize_ = false;
  gfx::Size size_;
  bool use_gpu_fence_;
  unsigned gpu_fence_id_ = 0;
  // Whether to send OutputSurfaceClient::DidSwapWithSize notifications.
  bool needs_swap_size_notifications_ = false;

  base::WeakPtrFactory<GLOutputSurface> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_H_
