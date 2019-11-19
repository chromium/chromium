// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/scheduler_delegate_win.h"

#include "chrome/browser/vr/scheduler_browser_renderer_interface.h"

namespace vr {

SchedulerDelegateWin::SchedulerDelegateWin() = default;
SchedulerDelegateWin::~SchedulerDelegateWin() {}

void SchedulerDelegateWin::OnPose(base::OnceCallback<void()> on_frame_ended,
                                  gfx::Transform head_pose,
                                  bool draw_overlay,
                                  bool draw_ui) {
  on_frame_ended_ = std::move(on_frame_ended);
  base::TimeTicks now = base::TimeTicks::Now();
  if (draw_overlay)
    browser_renderer_->DrawWebXrFrame(now, head_pose);
  else if (draw_ui)
    browser_renderer_->DrawBrowserFrame(now);
}

void SchedulerDelegateWin::OnPause() {
  NOTREACHED();
}

void SchedulerDelegateWin::OnResume() {
  NOTREACHED();
}

void SchedulerDelegateWin::OnExitPresent() {
  NOTREACHED();
}

void SchedulerDelegateWin::SetWebXrMode(bool enabled) {
  NOTREACHED();
}

void SchedulerDelegateWin::SetShowingVrDialog(bool showing) {
  NOTREACHED();
}

void SchedulerDelegateWin::SetBrowserRenderer(
    SchedulerBrowserRendererInterface* browser_renderer) {
  browser_renderer_ = browser_renderer;
}

void SchedulerDelegateWin::SubmitDrawnFrame(FrameType frame_type,
                                            const gfx::Transform& head_pose) {
  std::move(on_frame_ended_).Run();
}

void SchedulerDelegateWin::AddInputSourceState(
    device::mojom::XRInputSourceStatePtr state) {
  NOTREACHED();
}

void SchedulerDelegateWin::ConnectPresentingService(
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  NOTREACHED();
}

}  // namespace vr
