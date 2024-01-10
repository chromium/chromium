// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_BASE_SCHEDULER_DELEGATE_H_
#define CHROME_BROWSER_VR_BASE_SCHEDULER_DELEGATE_H_

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/vr/scheduler_delegate.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/util/fps_meter.h"

namespace base {
class TaskRunner;
}

namespace vr {

class SchedulerUiInterface;

class VR_EXPORT BaseSchedulerDelegate : public SchedulerDelegate {
 public:
  BaseSchedulerDelegate(SchedulerUiInterface* ui,
                        int webxr_spinner_timeout,
                        int webxr_initial_frame_timeout);

  BaseSchedulerDelegate(const BaseSchedulerDelegate&) = delete;
  BaseSchedulerDelegate& operator=(const BaseSchedulerDelegate&) = delete;

  ~BaseSchedulerDelegate() override;

  // SchedulerDelegate implementations.
  void OnExitPresent() override;

 protected:
  void ScheduleWebXrFrameTimeout();
  void CancelWebXrFrameTimeout();
  void OnNewWebXrFrame();

  uint64_t webxr_frames_received() const { return webxr_frames_received_; }
  void TickWebXrFramesReceived() { ++webxr_frames_received_; }
  void ResetWebXrFramesReceived() { webxr_frames_received_ = 0; }

  base::TaskRunner* task_runner() { return task_runner_.get(); }

 private:
  raw_ptr<SchedulerUiInterface> ui_;

  int webxr_frames_received_ = 0;
  int webxr_spinner_timeout_seconds_;
  int webxr_initial_frame_timeout_seconds_;

  device::FPSMeter webxr_fps_meter_;

  base::CancelableOnceClosure webxr_frame_timeout_closure_;
  base::CancelableOnceClosure webxr_spinner_timeout_closure_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_BASE_SCHEDULER_DELEGATE_H_
