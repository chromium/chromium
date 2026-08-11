// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_xr_input_source.h"

#include <cstddef>
#include <utility>

#include "base/check.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/test/webxr_test_gamepad_utils.h"
#include "ui/gfx/geometry/decomposed_transform.h"

namespace {
device::GamepadHand ToGamepadHand(device::mojom::XRHandedness handedness) {
  switch (handedness) {
    case device::mojom::XRHandedness::LEFT:
      return device::GamepadHand::kLeft;
    case device::mojom::XRHandedness::RIGHT:
      return device::GamepadHand::kRight;
    default:
      return device::GamepadHand::kNone;
  }
}
}  // namespace

MockXRInputSource::MockXRInputSource(MockXRDeviceHookBase* hook,
                                     device::mojom::XRHandedness handedness,
                                     bool has_hand_tracking)
    : hook_(hook), handedness_(handedness) {
  {
    base::AutoLock auto_lock(lock_);
    data_.is_valid = true;
    data_.pose_data = gfx::Transform();
    data_.handedness = handedness_;
    data_.gamepad = device::mojom::Gamepad::New();
    data_.gamepad->hand = ToGamepadHand(handedness_);
    data_.gamepad->mapping = device::GamepadMapping::kXrStandard;
    data_.gamepad->connected = true;

    auto max_button_index =
        device::GamepadButtonIndexFromButtonId(device::XrButtonId::kMax);
    CHECK(max_button_index.has_value());
    size_t num_buttons = *max_button_index + 1;
    data_.gamepad->buttons.reserve(num_buttons);
    for (size_t i = 0; i < num_buttons; ++i) {
      auto& button = data_.gamepad->buttons.emplace_back();
      button.used = true;
      button.type = device::GamepadButtonType::kStandard;
    }
    data_.gamepad->axes = {0.0, 0.0, 0.0, 0.0};
  }

  if (has_hand_tracking) {
    SetDefaultHandData();
  }
}

MockXRInputSource::~MockXRInputSource() = default;

void MockXRInputSource::SetConnected(bool connected) {
  base::AutoLock auto_lock(lock_);
  data_.is_valid = connected;
  if (data_.gamepad) {
    data_.gamepad->connected = connected;
  }
}

void MockXRInputSource::SetPose(const gfx::Transform& pose) {
  base::AutoLock auto_lock(lock_);
  data_.pose_data = pose;
}

void MockXRInputSource::SetHandedness(device::mojom::XRHandedness handedness) {
  base::AutoLock auto_lock(lock_);
  handedness_ = handedness;
  data_.handedness = handedness;
  if (data_.gamepad) {
    data_.gamepad->hand = ToGamepadHand(handedness);
  }
}

void MockXRInputSource::SetGamepad(device::mojom::GamepadPtr gamepad) {
  base::AutoLock auto_lock(lock_);
  data_.gamepad = std::move(gamepad);
}

void MockXRInputSource::ClearGamepad() {
  base::AutoLock auto_lock(lock_);
  data_.gamepad = nullptr;
}

void MockXRInputSource::SetTrigger(bool pressed, bool touched, double value) {
  SetButton(device::XrButtonId::kAxisTrigger, pressed, touched, value);
}

void MockXRInputSource::PressTrigger() {
  PressButton(device::XrButtonId::kAxisTrigger);
}

void MockXRInputSource::ReleaseTrigger() {
  ReleaseButton(device::XrButtonId::kAxisTrigger);
}

void MockXRInputSource::PressReleaseTrigger(uint32_t wait_frames) {
  PressReleaseButton(device::XrButtonId::kAxisTrigger, wait_frames);
}

void MockXRInputSource::PressButton(device::XrButtonId button) {
  SetButton(button, /*pressed=*/true, /*touched=*/true, 1.0);
}

void MockXRInputSource::ReleaseButton(device::XrButtonId button) {
  SetButton(button, /*pressed=*/false, /*touched=*/false, 0.0);
}

void MockXRInputSource::PressReleaseButton(device::XrButtonId button,
                                           uint32_t wait_frames) {
  PressButton(button);
  hook_->WaitNumFrames(wait_frames);
  ReleaseButton(button);
  hook_->WaitNumFrames(wait_frames);
}

void MockXRInputSource::SetButton(device::XrButtonId button_id,
                                  bool pressed,
                                  bool touched,
                                  double value) {
  base::AutoLock auto_lock(lock_);
  auto* button = device::GetGamepadButton(data_.gamepad.get(), button_id);
  if (button) {
    button->used = true;
    button->pressed = pressed;
    button->touched = touched;
    button->value = value;
  }
}

void MockXRInputSource::SetSupportedButtons(
    base::span<const device::XrButtonId> supported_buttons) {
  base::AutoLock auto_lock(lock_);
  if (!data_.gamepad) {
    return;
  }
  for (auto& button : data_.gamepad->buttons) {
    button.used = false;
  }
  for (device::XrButtonId button_id : supported_buttons) {
    if (auto* button =
            device::GetGamepadButton(data_.gamepad.get(), button_id)) {
      button->used = true;
    }
  }
}

void MockXRInputSource::SetAxis(device::XrButtonId button_id,
                                float x,
                                float y) {
  base::AutoLock auto_lock(lock_);
  auto start_index = device::GamepadAxisStartIndexFromButtonId(button_id);
  auto required_size = device::RequiredGamepadAxesSizeFromButtonId(button_id);
  if (data_.gamepad && start_index && required_size &&
      data_.gamepad->axes.size() >= *required_size) {
    data_.gamepad->axes[*start_index] = x;
    data_.gamepad->axes[*start_index + 1] = y;
  }
}

void MockXRInputSource::SetHandTrackingData(
    device::mojom::XRHandTrackingDataPtr hand_data) {
  base::AutoLock auto_lock(lock_);
  data_.hand_data = std::move(hand_data);
}

void MockXRInputSource::SetDefaultHandData(gfx::Quaternion orientation) {
  base::AutoLock auto_lock(lock_);
  auto hand_data = device::mojom::XRHandTrackingData::New();
  constexpr size_t num_joints =
      static_cast<size_t>(device::mojom::XRHandJoint::kMaxValue) + 1;
  hand_data->hand_joint_data.reserve(num_joints);

  gfx::DecomposedTransform decomposed_transform;
  decomposed_transform.quaternion = orientation;
  for (size_t i = 0; i < num_joints; ++i) {
    decomposed_transform.translate[0] = i / 100.0;
    auto joint_data = device::mojom::XRHandJointData::New();
    joint_data->joint = static_cast<device::mojom::XRHandJoint>(i);
    joint_data->mojo_from_joint = gfx::Transform::Compose(decomposed_transform);
    joint_data->radius = static_cast<float>(i);
    hand_data->hand_joint_data.push_back(std::move(joint_data));
  }
  data_.hand_data = std::move(hand_data);
}

void MockXRInputSource::ClearHandData() {
  base::AutoLock auto_lock(lock_);
  data_.hand_data = nullptr;
}

device::ControllerFrameData MockXRInputSource::GetFrameData() const {
  base::AutoLock auto_lock(lock_);
  device::ControllerFrameData copy;
  copy.is_valid = data_.is_valid;
  copy.handedness = data_.handedness;
  copy.pose_data = data_.pose_data;
  if (data_.gamepad) {
    copy.gamepad = data_.gamepad.Clone();
  }
  if (data_.hand_data) {
    copy.hand_data = data_.hand_data.Clone();
  }
  return copy;
}
