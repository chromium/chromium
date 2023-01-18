// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_INPUT_DELEGATE_WIN_H_
#define CHROME_BROWSER_VR_WIN_INPUT_DELEGATE_WIN_H_

#include "chrome/browser/vr/input_delegate.h"

namespace vr {

class InputDelegateWin : public InputDelegate {
 public:
  ~InputDelegateWin() override;

  // Push input state (head pose, current_time)
  void OnPose(gfx::Transform head_pose_for_current_frame) {
    head_pose_for_current_frame_ = head_pose_for_current_frame;
  }

 private:
  gfx::Transform GetHeadPose() override;
  void OnTriggerEvent(bool pressed) override;
  void UpdateController(const gfx::Transform& head_pose,
                        base::TimeTicks current_time,
                        bool is_webxr_frame) override;
  InputEventList GetGestures(base::TimeTicks current_time) override;
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override;
  void OnResume() override;
  void OnPause() override;

  gfx::Transform head_pose_for_current_frame_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_INPUT_DELEGATE_WIN_H_
