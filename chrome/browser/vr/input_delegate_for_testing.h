// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_INPUT_DELEGATE_FOR_TESTING_H_
#define CHROME_BROWSER_VR_INPUT_DELEGATE_FOR_TESTING_H_

#include <queue>

#include "base/macros.h"
#include "chrome/browser/vr/gesture_detector.h"
#include "chrome/browser/vr/input_delegate.h"
#include "chrome/browser/vr/model/controller_model.h"

namespace vr {

class UiInterface;
struct ControllerTestInput;

class InputDelegateForTesting : public InputDelegate {
 public:
  explicit InputDelegateForTesting(UiInterface* ui);
  ~InputDelegateForTesting() override;

  void QueueControllerActionForTesting(ControllerTestInput controller_input);
  bool IsQueueEmpty() const;

  // InputDelegate implementation.
  gfx::Transform GetHeadPose() override;
  void OnTriggerEvent(bool pressed) override;
  void UpdateController(const gfx::Transform& head_pose,
                        base::TimeTicks current_time,
                        bool is_webxr_frame) override;
  ControllerModel GetControllerModel(const gfx::Transform& head_pose) override;
  InputEventList GetGestures(base::TimeTicks current_time) override;
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override;
  void OnResume() override;
  void OnPause() override;

 private:
  ControllerModel GetMostRecentModel();

  UiInterface* ui_;
  std::queue<ControllerModel> controller_model_queue_;
  ControllerModel cached_controller_model_;
  ControllerModel previous_controller_model_;
  base::TimeTicks last_touchpad_timestamp_;
  std::unique_ptr<GestureDetector> gesture_detector_;

  DISALLOW_COPY_AND_ASSIGN(InputDelegateForTesting);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_INPUT_DELEGATE_FOR_TESTING_H_
