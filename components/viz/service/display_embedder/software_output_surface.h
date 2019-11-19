// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_SURFACE_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/latency/latency_info.h"
#include "ui/latency/latency_tracker.h"

namespace viz {
class SoftwareOutputDevice;

class VIZ_SERVICE_EXPORT SoftwareOutputSurface : public OutputSurface {
 public:
  explicit SoftwareOutputSurface(
      std::unique_ptr<SoftwareOutputDevice> software_device);
  ~SoftwareOutputSurface() override;

  // OutputSurface implementation.
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
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  unsigned UpdateGpuFence() override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
#endif

 private:
  void SwapBuffersCallback(base::TimeTicks swap_time,
                           const gfx::Size& pixel_size);
  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);

  OutputSurfaceClient* client_ = nullptr;

  UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  base::TimeTicks refresh_timebase_;
  base::TimeDelta refresh_interval_ = BeginFrameArgs::DefaultInterval();

  std::queue<std::vector<ui::LatencyInfo>> stored_latency_info_;
  ui::LatencyTracker latency_tracker_;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  bool needs_swap_size_notifications_ = false;
#endif

  base::WeakPtrFactory<SoftwareOutputSurface> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputSurface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_SURFACE_H_
