// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gamepad.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/logging.h"

namespace exo {

Gamepad::Gamepad(const ui::GamepadDevice& gamepad_device)
    : device(gamepad_device),
      input_controller_(
          ui::OzonePlatform::GetInstance()->GetInputController()) {}

Gamepad::~Gamepad() {
  for (GamepadObserver& observer : observer_list_)
    observer.OnGamepadDestroying(this);

  if (delegate_)
    delegate_->OnRemoved();
}

void Gamepad::Vibrate(const std::vector<int64_t>& duration_millis,
                      const std::vector<uint8_t>& amplitudes,
                      int32_t repeat) {
  if (!device.supports_vibration_rumble) {
    VLOG(2) << "Vibrate failed because gamepad does not support vibration.";
    return;
  }

  if (duration_millis.size() != amplitudes.size()) {
    VLOG(2) << "Vibrate failed because the amplitudes vector and "
               "duration_millis vector are not the same size.";
    return;
  }

  vibration_timer_.Stop();
  vibration_timer_.Start(
      FROM_HERE, base::Milliseconds(0),
      base::BindOnce(&Gamepad::HandleVibrate, base::Unretained(this),
                     duration_millis, amplitudes, repeat, /*index=*/0,
                     /*duration_already_vibrated=*/0));
}

void Gamepad::HandleVibrate(const std::vector<int64_t>& duration_millis,
                            const std::vector<uint8_t>& amplitudes,
                            int32_t repeat,
                            size_t index,
                            int64_t duration_already_vibrated) {
  size_t vector_size = duration_millis.size();
  if (index >= vector_size)
    return;

  if (!can_vibrate_) {
    VLOG(2) << "Gamepad is not allowed to vibrate because it is not in focus.";
    return;
  }

  int64_t duration_left_to_vibrate =
      duration_millis[index] - duration_already_vibrated;

  if (duration_left_to_vibrate > kMaxDurationMillis) {
    //  The device does not support effects this long. Issue periodic vibration
    //  commands until the effect is complete.
    SendVibrate(amplitudes[index], kMaxDurationMillis);
    vibration_timer_.Start(
        FROM_HERE, base::Milliseconds(kMaxDurationMillis),
        base::BindOnce(&Gamepad::HandleVibrate, base::Unretained(this),
                       duration_millis, amplitudes, repeat, index,
                       /*duration_already_vibrated=*/duration_already_vibrated +
                           kMaxDurationMillis));
  } else {
    SendVibrate(amplitudes[index], duration_left_to_vibrate);
    index++;
    bool needs_to_repeat = index >= vector_size && repeat >= 0 &&
                           repeat < static_cast<int32_t>(vector_size);
    if (needs_to_repeat)
      index = repeat;

    vibration_timer_.Start(
        FROM_HERE, base::Milliseconds(duration_left_to_vibrate),
        base::BindOnce(&Gamepad::HandleVibrate, base::Unretained(this),
                       duration_millis, amplitudes, repeat, index,
                       /*duration_already_vibrated=*/0));
  }
}

void Gamepad::SendVibrate(uint8_t amplitude, int64_t duration_millis) {
  // |duration_millis| is always <= |kMaxDurationMillis|, which is the max value
  // for uint16_t, so it is safe to cast it to uint16_t here.
  input_controller_->PlayVibrationEffect(
      device.id, amplitude, static_cast<uint16_t>(duration_millis));
}

void Gamepad::CancelVibration() {
  if (!device.supports_vibration_rumble) {
    VLOG(2)
        << "CancelVibration failed because gamepad does not support vibration.";
    return;
  }

  if (!vibration_timer_.IsRunning())
    return;

  vibration_timer_.Stop();
  SendCancelVibration();
}

void Gamepad::SendCancelVibration() {
  input_controller_->StopVibration(device.id);
}

void Gamepad::SetDelegate(std::unique_ptr<GamepadDelegate> delegate) {
  DCHECK(!delegate_);
  delegate_ = std::move(delegate);
}

void Gamepad::AddObserver(GamepadObserver* observer) {
  observer_list_.AddObserver(observer);
}

bool Gamepad::HasObserver(GamepadObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Gamepad::RemoveObserver(GamepadObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void Gamepad::OnGamepadFocused() {
  can_vibrate_ = base::FeatureList::IsEnabled(ash::features::kGamepadVibration);
}

void Gamepad::OnGamepadFocusLost() {
  can_vibrate_ = false;
  CancelVibration();
}

void Gamepad::OnGamepadEvent(const ui::GamepadEvent& event) {
  DCHECK(delegate_);
  switch (event.type()) {
    case ui::GamepadEventType::BUTTON:
      delegate_->OnButton(event.code(), event.value(), event.timestamp());
      break;
    case ui::GamepadEventType::AXIS:
      delegate_->OnAxis(event.code(), event.value(), event.timestamp());
      break;
    case ui::GamepadEventType::FRAME:
      delegate_->OnFrame(event.timestamp());
      break;
  }
}

}  // namespace exo
