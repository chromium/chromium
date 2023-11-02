// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_INPUT_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace gfx {
class Transform;
}

namespace vr {

class InputEvent;
struct ControllerModel;

using InputEventList = std::vector<std::unique_ptr<InputEvent>>;

// Obtains input from the controller and head poses from the headset.
class InputDelegate {
 public:
  virtual ~InputDelegate() {}

  virtual gfx::Transform GetHeadPose() = 0;
  virtual void OnTriggerEvent(bool pressed) = 0;
  virtual void UpdateController(const gfx::Transform& head_pose,
                                base::TimeTicks current_time,
                                bool is_webxr_frame) = 0;
  virtual ControllerModel GetControllerModel(
      const gfx::Transform& head_pose) = 0;
  virtual InputEventList GetGestures(base::TimeTicks current_time) = 0;
  virtual device::mojom::XRInputSourceStatePtr GetInputSourceState() = 0;
  virtual void OnResume() = 0;
  virtual void OnPause() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_INPUT_DELEGATE_H_
