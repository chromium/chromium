// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_VSYNC_BEGIN_FRAME_SOURCE_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_VSYNC_BEGIN_FRAME_SOURCE_H_

#include "base/macros.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
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
  ~GpuVSyncBeginFrameSource() override;

  // ExternalBeginFrameSource overrides.
  BeginFrameArgs GetMissedBeginFrameArgs(BeginFrameObserver* obs) override;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

 private:
  void OnGpuVSync(base::TimeTicks vsync_time, base::TimeDelta vsync_interval);

  OutputSurface* const output_surface_;
  uint64_t next_begin_frame_sequence_number_ = 1;

  DISALLOW_COPY_AND_ASSIGN(GpuVSyncBeginFrameSource);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_VSYNC_BEGIN_FRAME_SOURCE_H_
