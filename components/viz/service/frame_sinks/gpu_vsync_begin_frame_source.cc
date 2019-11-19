// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/gpu_vsync_begin_frame_source.h"

#include "base/bind.h"
#include "components/viz/service/display/output_surface.h"

namespace viz {

GpuVSyncBeginFrameSource::GpuVSyncBeginFrameSource(
    uint32_t restart_id,
    OutputSurface* output_surface)
    : ExternalBeginFrameSource(this, restart_id),
      output_surface_(output_surface) {
  DCHECK(output_surface->capabilities().supports_gpu_vsync);
  output_surface->SetGpuVSyncCallback(base::BindRepeating(
      &GpuVSyncBeginFrameSource::OnGpuVSync, base::Unretained(this)));
}

GpuVSyncBeginFrameSource::~GpuVSyncBeginFrameSource() = default;

void GpuVSyncBeginFrameSource::OnGpuVSync(base::TimeTicks vsync_time,
                                          base::TimeDelta vsync_interval) {
  ExternalBeginFrameSource::OnBeginFrame(BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), next_begin_frame_sequence_number_++,
      vsync_time, vsync_time + vsync_interval, vsync_interval,
      BeginFrameArgs::NORMAL));
}

BeginFrameArgs GpuVSyncBeginFrameSource::GetMissedBeginFrameArgs(
    BeginFrameObserver* obs) {
  auto frame_time = last_begin_frame_args_.frame_time;
  auto interval = last_begin_frame_args_.interval;
  auto now = base::TimeTicks::Now();

  if (last_begin_frame_args_.IsValid()) {
    frame_time = now.SnappedToNextTick(frame_time, interval) - interval;
  } else {
    // Create BeginFrameArgs for now so that we don't have to wait until vsync.
    frame_time = now;
    interval = BeginFrameArgs::DefaultInterval();
  }

  // Don't create new args unless we've actually moved past the previous frame.
  if (!last_begin_frame_args_.IsValid() ||
      frame_time > last_begin_frame_args_.frame_time) {
    last_begin_frame_args_ = BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, source_id(), next_begin_frame_sequence_number_++,
        frame_time, frame_time + interval, interval, BeginFrameArgs::NORMAL);
  }

  return ExternalBeginFrameSource::GetMissedBeginFrameArgs(obs);
}

void GpuVSyncBeginFrameSource::OnNeedsBeginFrames(bool needs_begin_frames) {
  output_surface_->SetGpuVSyncEnabled(needs_begin_frames);
}

}  // namespace viz
