// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/input_delegate_win.h"

#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"

namespace vr {

InputDelegateWin::~InputDelegateWin() {}

gfx::Transform InputDelegateWin::GetHeadPose() {
  return head_pose_for_current_frame_;
}

void InputDelegateWin::OnTriggerEvent(bool pressed) {
  // No input currently processed for desktop.
}

void InputDelegateWin::UpdateController(const gfx::Transform& head_pose,
                                        base::TimeTicks current_time,
                                        bool is_webxr_frame) {
  // No input currently processed for desktop.
}

InputEventList InputDelegateWin::GetGestures(base::TimeTicks current_time) {
  return {};
}

device::mojom::XRInputSourceStatePtr InputDelegateWin::GetInputSourceState() {
  NOTREACHED();
  device::mojom::XRInputSourceStatePtr state;
  return state;
}

void InputDelegateWin::OnResume() {
  NOTREACHED();
}

void InputDelegateWin::OnPause() {
  NOTREACHED();
}

}  // namespace vr
