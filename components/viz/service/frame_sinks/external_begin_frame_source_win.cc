// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_win.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"

namespace viz {

namespace {

BASE_FEATURE(kExternalBeginFrameSourceWinUsesRunOrPostTask,
             "ExternalBeginFrameSourceWinUsesRunOrPostTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

ExternalBeginFrameSourceWin::ExternalBeginFrameSourceWin(
    uint32_t restart_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExternalBeginFrameSource(this, restart_id),
      task_runner_(std::move(task_runner)) {}

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
    base::TimeDelta vsync_interval) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  vsync_interval_ = vsync_interval;
  if (skip_next_vsync_) {
    TRACE_EVENT_INSTANT0("gpu",
                         "ExternalBeginFrameSourceWin::OnVSync - skip_vsync",
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
        source_id(), frame_time, frame_time + interval, interval);
  }

  return ExternalBeginFrameSource::GetMissedBeginFrameArgs(obs);
}

void ExternalBeginFrameSourceWin::SetPreferredInterval(
    base::TimeDelta interval) {
  auto interval_for_half_refresh_rate = vsync_interval_ * 2;
  constexpr auto kMaxDelta = base::Milliseconds(0.5);
  bool run_at_half_refresh_rate =
      interval > (interval_for_half_refresh_rate - kMaxDelta);
  if (run_at_half_refresh_rate_ == run_at_half_refresh_rate)
    return;

  TRACE_EVENT1("gpu", "ExternalBeginFrameSourceWin::SetPreferredInterval",
               "run_at_half_refresh_rate", run_at_half_refresh_rate);
  run_at_half_refresh_rate_ = run_at_half_refresh_rate;
  skip_next_vsync_ = false;
}

void ExternalBeginFrameSourceWin::OnNeedsBeginFrames(bool needs_begin_frames) {
  if (observing_vsync_ == needs_begin_frames) {
    return;
  }
  observing_vsync_ = needs_begin_frames;
  skip_next_vsync_ = false;
  if (needs_begin_frames) {
    gl::VSyncThreadWin::GetInstance()->AddObserver(this);
  } else {
    gl::VSyncThreadWin::GetInstance()->RemoveObserver(this);
  }
}

void ExternalBeginFrameSourceWin::SetVSyncDisplayID(int64_t display_id) {
  // TODO(sunnyps): See if we should use non-primary displays for driving vsync.
}

}  // namespace viz
