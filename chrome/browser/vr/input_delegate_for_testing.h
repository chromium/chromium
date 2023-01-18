// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_INPUT_DELEGATE_FOR_TESTING_H_
#define CHROME_BROWSER_VR_INPUT_DELEGATE_FOR_TESTING_H_

#include <queue>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/vr/gesture_detector.h"
#include "chrome/browser/vr/input_delegate.h"
#include "chrome/browser/vr/model/controller_model.h"

namespace vr {

class UiInterface;
struct ControllerTestInput;

class InputDelegateForTesting : public InputDelegate {
 public:
  explicit InputDelegateForTesting(UiInterface* ui);

  InputDelegateForTesting(const InputDelegateForTesting&) = delete;
  InputDelegateForTesting& operator=(const InputDelegateForTesting&) = delete;

  ~InputDelegateForTesting() override;

  void QueueControllerActionForTesting(ControllerTestInput controller_input);
  bool IsQueueEmpty() const;

  // InputDelegate implementation.
  gfx::Transform GetHeadPose() override;
  void OnTriggerEvent(bool pressed) override;
  void UpdateController(const gfx::Transform& head_pose,
                        base::TimeTicks current_time,
                        bool is_webxr_frame) override;
  InputEventList GetGestures(base::TimeTicks current_time) override;
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override;
  void OnResume() override;
  void OnPause() override;

 private:
  ControllerModel GetMostRecentModel();

  raw_ptr<UiInterface> ui_;
  std::queue<ControllerModel> controller_model_queue_;
  ControllerModel cached_controller_model_;
  ControllerModel previous_controller_model_;
  base::TimeTicks last_touchpad_timestamp_;
  std::unique_ptr<GestureDetector> gesture_detector_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_INPUT_DELEGATE_FOR_TESTING_H_
