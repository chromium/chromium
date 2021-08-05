// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/base_scheduler_delegate.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/scheduler_ui_interface.h"

namespace vr {

BaseSchedulerDelegate::BaseSchedulerDelegate(SchedulerUiInterface* ui,
                                             bool start_in_webxr_mode,
                                             int webxr_spinner_timeout,
                                             int webxr_initial_frame_timeout)
    : ui_(ui),
      webxr_mode_(start_in_webxr_mode),
      webxr_spinner_timeout_seconds_(webxr_spinner_timeout),
      webxr_initial_frame_timeout_seconds_(webxr_initial_frame_timeout),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

BaseSchedulerDelegate::~BaseSchedulerDelegate() = default;

void BaseSchedulerDelegate::OnExitPresent() {
  CancelWebXrFrameTimeout();
}

void BaseSchedulerDelegate::SetWebXrMode(bool enabled) {
  if (webxr_mode_ == enabled)
    return;
  webxr_mode_ = enabled;
  if (webxr_mode_)
    ResetWebXrFramesReceived();
  else
    CancelWebXrFrameTimeout();
}

void BaseSchedulerDelegate::ScheduleWebXrFrameTimeout() {
  DCHECK(ui_);
  webxr_spinner_timeout_closure_.Reset(base::BindOnce(
      &SchedulerUiInterface::OnWebXrTimeoutImminent, base::Unretained(ui_)));
  task_runner_->PostDelayedTask(
      FROM_HERE, webxr_spinner_timeout_closure_.callback(),
      base::TimeDelta::FromSeconds(webxr_spinner_timeout_seconds_));
  webxr_frame_timeout_closure_.Reset(base::BindOnce(
      &SchedulerUiInterface::OnWebXrTimedOut, base::Unretained(ui_)));
  task_runner_->PostDelayedTask(
      FROM_HERE, webxr_frame_timeout_closure_.callback(),
      base::TimeDelta::FromSeconds(webxr_initial_frame_timeout_seconds_));
}

void BaseSchedulerDelegate::CancelWebXrFrameTimeout() {
  if (!webxr_spinner_timeout_closure_.IsCancelled())
    webxr_spinner_timeout_closure_.Cancel();
  if (!webxr_frame_timeout_closure_.IsCancelled())
    webxr_frame_timeout_closure_.Cancel();
}

void BaseSchedulerDelegate::OnNewWebXrFrame() {
  ui_->OnWebXrFrameAvailable();

  if (webxr_mode_) {
    TickWebXrFramesReceived();

    webxr_fps_meter_.AddFrame(base::TimeTicks::Now());
    TRACE_COUNTER1("gpu", "WebVR FPS", webxr_fps_meter_.GetFPS());
  }

  CancelWebXrFrameTimeout();
}

}  // namespace vr
