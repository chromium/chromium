// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_SURFACE_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class SoftwareOutputDevice;

class VIZ_SERVICE_EXPORT SoftwareOutputSurface : public OutputSurface {
 public:
  explicit SoftwareOutputSurface(
      std::unique_ptr<SoftwareOutputDevice> software_device);

  SoftwareOutputSurface(const SoftwareOutputSurface&) = delete;
  SoftwareOutputSurface& operator=(const SoftwareOutputSurface&) = delete;

  ~SoftwareOutputSurface() override;

  // OutputSurface implementation.
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const ReshapeParams& params) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
#endif

 private:
  void SwapBuffersCallback(base::TimeTicks swap_time,
                           int64_t swap_trace_id,
                           const gfx::Size& pixel_size);
  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);

  raw_ptr<OutputSurfaceClient> client_ = nullptr;

  UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  base::TimeTicks refresh_timebase_;
  base::TimeDelta refresh_interval_ = BeginFrameArgs::DefaultInterval();

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  bool needs_swap_size_notifications_ = false;
#endif

  base::WeakPtrFactory<SoftwareOutputSurface> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_SURFACE_H_
