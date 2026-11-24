// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_win.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/vsync_provider_win.h"

namespace viz {

namespace {

BASE_FEATURE(kExternalBeginFrameSourceWinUsesRunOrPostTask,
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::TimeDelta kIntervalTolerance = base::Milliseconds(0.5);

base::TimeDelta QueryWindowVSyncInterval(gfx::AcceleratedWidget window) {
  if (window == gfx::kNullAcceleratedWidget) {
    return base::TimeDelta();
  }
  base::TimeDelta interval;
  if (gl::VSyncProviderWin(window).GetVSyncIntervalIfAvailable(&interval) &&
      interval.is_positive()) {
    return interval;
  }
  return base::TimeDelta();
}

}  // namespace

ExternalBeginFrameSourceWin::ExternalBeginFrameSourceWin(
    uint32_t restart_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    gfx::AcceleratedWidget window)
    : ExternalBeginFrameSource(this, restart_id),
      task_runner_(std::move(task_runner)),
      window_(window),
      hardware_vsync_interval_(QueryWindowVSyncInterval(window)) {}

ExternalBeginFrameSourceWin::~ExternalBeginFrameSourceWin() {
  if (observing_vsync_) {
    gl::VSyncThreadWin::GetInstance()->RemoveObserver(this);
  }
}

void ExternalBeginFrameSourceWin::OnVSync(base::TimeTicks vsync_time,
                                          base::TimeDelta vsync_interval) {
  auto callback =
      base::BindOnce(&ExternalBeginFrameSourceWin::OnVSyncOnSequence,
                     weak_factory_.GetWeakPtr(), vsync_time, vsync_interval);
  if (base::FeatureList::IsEnabled(
          kExternalBeginFrameSourceWinUsesRunOrPostTask)) {
    task_runner_->RunOrPostTask(base::subtle::RunOrPostTaskPassKey(), FROM_HERE,
                                std::move(callback));
  } else {
    task_runner_->PostTask(FROM_HERE, std::move(callback));
  }
}

void ExternalBeginFrameSourceWin::OnVSyncOnSequence(
    base::TimeTicks vsync_time,
    base::TimeDelta vsync_thread_interval) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const base::TimeDelta unthrottled_interval =
      AdjustVSyncInterval(vsync_thread_interval);
  if (vsync_thread_interval_ != vsync_thread_interval) {
    vsync_thread_interval_ = vsync_thread_interval;
    CheckRunAtHalfRefreshRate(unthrottled_interval);
  }

  const base::TimeDelta target_interval = run_at_half_refresh_rate_
                                              ? unthrottled_interval * 2
                                              : unthrottled_interval;

  if (target_interval > vsync_thread_interval + kIntervalTolerance) {
    // TODO(crbug.com/523555264): Simulating a slower monitor's vblank cadence
    // from the single global VSync thread is still not 100% phase-accurate
    // compared to waiting on each monitor's actual vblank (WaitForVBlank).
    if (last_expected_vblank_time_.is_null()) {
      last_expected_vblank_time_ = vsync_time;
    } else {
      const base::TimeDelta elapsed = vsync_time - last_expected_vblank_time_;
      if (elapsed + kIntervalTolerance < target_interval) {
        TRACE_EVENT_INSTANT(
            "gpu", "ExternalBeginFrameSourceWin::OnVSync - skip_vsync");
        return;
      }
      // Snap to the latest expected vblank at or before vsync_time.
      const int64_t num_intervals =
          (elapsed + kIntervalTolerance).IntDiv(target_interval);
      last_expected_vblank_time_ += target_interval * num_intervals;
    }
  } else {
    last_expected_vblank_time_ = base::TimeTicks();
  }

  auto begin_frame_args = begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), vsync_time, vsync_time + target_interval, target_interval,
      unthrottled_interval);
  ExternalBeginFrameSource::OnBeginFrame(begin_frame_args);
}

BeginFrameArgs ExternalBeginFrameSourceWin::GetMissedBeginFrameArgs(
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
        source_id(), frame_time, frame_time + interval, interval, interval);
  }

  return ExternalBeginFrameSource::GetMissedBeginFrameArgs(obs);
}

void ExternalBeginFrameSourceWin::SetPreferredInterval(
    base::TimeDelta interval) {
  preferred_interval_ = interval;
  CheckRunAtHalfRefreshRate(AdjustVSyncInterval(vsync_thread_interval_));
  TRACE_EVENT1("gpu", "ExternalBeginFrameSourceWin::SetPreferredInterval",
               "run_at_half_refresh_rate", run_at_half_refresh_rate_);
}

void ExternalBeginFrameSourceWin::OnNeedsBeginFrames(bool needs_begin_frames) {
  if (observing_vsync_ == needs_begin_frames) {
    return;
  }
  observing_vsync_ = needs_begin_frames;
  last_expected_vblank_time_ = base::TimeTicks();
  if (needs_begin_frames) {
    gl::VSyncThreadWin::GetInstance()->AddObserver(this);
  } else {
    gl::VSyncThreadWin::GetInstance()->RemoveObserver(this);
  }
}

void ExternalBeginFrameSourceWin::SetVSyncDisplayID(int64_t display_id,
                                                    bool force_update) {
  UpdateHardwareVSyncInterval();
}

base::TimeDelta ExternalBeginFrameSourceWin::QueryHardwareVSyncInterval()
    const {
  return QueryWindowVSyncInterval(window_);
}

void ExternalBeginFrameSourceWin::UpdateHardwareVSyncInterval() {
  const base::TimeDelta interval = QueryHardwareVSyncInterval();
  if (interval.is_positive() && hardware_vsync_interval_ != interval) {
    TRACE_EVENT1("gpu",
                 "ExternalBeginFrameSourceWin::UpdateHardwareVSyncInterval",
                 "hardware_vsync_interval_us", interval.InMicroseconds());
    hardware_vsync_interval_ = interval;
    last_expected_vblank_time_ = base::TimeTicks();
    CheckRunAtHalfRefreshRate(AdjustVSyncInterval(vsync_thread_interval_));
  }
}

base::TimeDelta ExternalBeginFrameSourceWin::AdjustVSyncInterval(
    base::TimeDelta vsync_thread_interval) const {
  // If the window is on a slower monitor, use its refresh interval. If the
  // window is on a faster monitor, use the VSync thread interval because the
  // VSync thread cannot emit VSync notifications faster than that.
  return std::max(vsync_thread_interval, hardware_vsync_interval_);
}

void ExternalBeginFrameSourceWin::CheckRunAtHalfRefreshRate(
    base::TimeDelta vsync_interval) {
  const base::TimeDelta interval_for_half_refresh_rate = vsync_interval * 2;
  const bool run_at_half_refresh_rate =
      preferred_interval_ >
      (interval_for_half_refresh_rate - kIntervalTolerance);
  if (run_at_half_refresh_rate_ == run_at_half_refresh_rate) {
    return;
  }

  run_at_half_refresh_rate_ = run_at_half_refresh_rate;
  last_expected_vblank_time_ = base::TimeTicks();
}

}  // namespace viz
