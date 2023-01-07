// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/gpu_vsync_begin_frame_source.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
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
  vsync_interval_ = vsync_interval;
  if (skip_next_vsync_) {
    TRACE_EVENT_INSTANT0("gpu",
                         "GpuVSyncBeginFrameSource::OnGpuVSync - skip_vsync",
                         TRACE_EVENT_SCOPE_THREAD);
    skip_next_vsync_ = false;
    return;
  }

  if (run_at_half_refresh_rate_) {
    skip_next_vsync_ = true;
    vsync_interval *= 2;
  }
  auto begin_frame_args = begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), vsync_time, vsync_time + vsync_interval, vsync_interval);
  ExternalBeginFrameSource::OnBeginFrame(begin_frame_args);
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
    last_begin_frame_args_ = begin_frame_args_generator_.GenerateBeginFrameArgs(
        source_id(), frame_time, frame_time + interval, interval);
  }

  return ExternalBeginFrameSource::GetMissedBeginFrameArgs(obs);
}

void GpuVSyncBeginFrameSource::SetPreferredInterval(base::TimeDelta interval) {
  auto interval_for_half_refresh_rate = vsync_interval_ * 2;
  constexpr auto kMaxDelta = base::Milliseconds(0.5);
  bool run_at_half_refresh_rate =
      interval > (interval_for_half_refresh_rate - kMaxDelta);
  if (run_at_half_refresh_rate_ == run_at_half_refresh_rate)
    return;

  TRACE_EVENT1("gpu", "GpuVSyncBeginFrameSource::SetPreferredInterval",
               "run_at_half_refresh_rate", run_at_half_refresh_rate);
  run_at_half_refresh_rate_ = run_at_half_refresh_rate;
  skip_next_vsync_ = false;
}

void GpuVSyncBeginFrameSource::SetDynamicBeginFrameDeadlineOffsetSource(
    DynamicBeginFrameDeadlineOffsetSource*
        dynamic_begin_frame_deadline_offset_source) {
  begin_frame_args_generator_.set_dynamic_begin_frame_deadline_offset_source(
      dynamic_begin_frame_deadline_offset_source);
}

void GpuVSyncBeginFrameSource::OnNeedsBeginFrames(bool needs_begin_frames) {
  skip_next_vsync_ = false;
  output_surface_->SetGpuVSyncEnabled(needs_begin_frames);
}

}  // namespace viz
