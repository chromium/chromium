// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_VSYNC_BEGIN_FRAME_SOURCE_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_VSYNC_BEGIN_FRAME_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/frame_rate_decider.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class OutputSurface;

// Receives begin frames via OutputSurface::SetGpuVSyncCallback().  Output
// surface must have |supports_gpu_vsync| capability.  This class is not thread
// safe so the callbacks must be received on the original thread.  The BFS is
// guaranteed to outlive the OutputSurface.
class VIZ_SERVICE_EXPORT GpuVSyncBeginFrameSource
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  GpuVSyncBeginFrameSource(uint32_t restart_id, OutputSurface* output_surface);

  GpuVSyncBeginFrameSource(const GpuVSyncBeginFrameSource&) = delete;
  GpuVSyncBeginFrameSource& operator=(const GpuVSyncBeginFrameSource&) = delete;

  ~GpuVSyncBeginFrameSource() override;

  // ExternalBeginFrameSource overrides.
  BeginFrameArgs GetMissedBeginFrameArgs(BeginFrameObserver* obs) override;
  void SetPreferredInterval(base::TimeDelta interval) override;
  void SetVSyncDisplayID(int64_t display_id) override;

  // BeginFrameSource:
  void SetDynamicBeginFrameDeadlineOffsetSource(
      DynamicBeginFrameDeadlineOffsetSource*
          dynamic_begin_frame_deadline_offset_source) override;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

 private:
  void OnGpuVSync(base::TimeTicks vsync_time, base::TimeDelta vsync_interval);

  const raw_ptr<OutputSurface, DanglingUntriaged> output_surface_;
  BeginFrameArgsGenerator begin_frame_args_generator_;

  bool run_at_half_refresh_rate_ = false;
  bool skip_next_vsync_ = false;
  base::TimeDelta vsync_interval_ = BeginFrameArgs::DefaultInterval();
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_VSYNC_BEGIN_FRAME_SOURCE_H_
