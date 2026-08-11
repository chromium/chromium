// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_XR_INPUT_SOURCE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_XR_INPUT_SOURCE_H_

#include <cstdint>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/vr/public/mojom/test/controller_frame_data.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "device/vr/test/webxr_test_gamepad_utils.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"

class MockXRDeviceHookBase;

// Represents an individual input source (controller or hand tracking)
// attached to a MockXRDeviceHookBase for WebXR browser tests.
class MockXRInputSource {
 public:
  MockXRInputSource(MockXRDeviceHookBase* hook,
                    device::mojom::XRHandedness handedness,
                    bool has_hand_tracking = false);
  ~MockXRInputSource();

  MockXRInputSource(const MockXRInputSource&) = delete;
  MockXRInputSource& operator=(const MockXRInputSource&) = delete;

  device::mojom::XRHandedness handedness() const { return handedness_; }

  void SetConnected(bool connected);
  void SetPose(const gfx::Transform& pose);
  void SetHandedness(device::mojom::XRHandedness handedness);
  void SetGamepad(device::mojom::GamepadPtr gamepad);
  void ClearGamepad();

  // Trigger helpers:
  void SetTrigger(bool pressed, bool touched, double value);
  void PressTrigger();
  void ReleaseTrigger();
  void PressReleaseTrigger(uint32_t wait_frames = 30);

  // Button helpers:
  void PressButton(device::XrButtonId button);
  void ReleaseButton(device::XrButtonId button);
  void PressReleaseButton(device::XrButtonId button, uint32_t wait_frames = 30);
  void SetButton(device::XrButtonId button_id,
                 bool pressed,
                 bool touched,
                 double value);
  void SetSupportedButtons(
      base::span<const device::XrButtonId> supported_buttons);

  // Axes helpers:
  void SetAxis(device::XrButtonId button_id, float x, float y);

  // Hand tracking helpers:
  void SetHandTrackingData(device::mojom::XRHandTrackingDataPtr hand_data);
  void SetDefaultHandData(gfx::Quaternion orientation = gfx::Quaternion());
  void ClearHandData();

  // Returns a copy of the current ControllerFrameData snapshot under lock.
  device::ControllerFrameData GetFrameData() const;

 private:
  const raw_ptr<MockXRDeviceHookBase> hook_;
  device::mojom::XRHandedness handedness_;

  mutable base::Lock lock_;
  device::ControllerFrameData data_ GUARDED_BY(lock_);
};

#endif  // CHROME_BROWSER_VR_TEST_MOCK_XR_INPUT_SOURCE_H_
