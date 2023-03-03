// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"

namespace ash::settings {

InputDeviceSettingsProvider::InputDeviceSettingsProvider() {
  auto* controller = InputDeviceSettingsController::Get();
  if (features::IsInputDeviceSettingsSplitEnabled() && controller) {
    controller->AddObserver(this);
  }
}

InputDeviceSettingsProvider::~InputDeviceSettingsProvider() {
  auto* controller = InputDeviceSettingsController::Get();
  if (features::IsInputDeviceSettingsSplitEnabled() && controller) {
    controller->RemoveObserver(this);
  }
}

void InputDeviceSettingsProvider::BindInterface(
    mojo::PendingReceiver<mojom::InputDeviceSettingsProvider> receiver) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void InputDeviceSettingsProvider::GetConnectedKeyboards(
    GetConnectedKeyboardsCallback callback) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  std::move(callback).Run(
      InputDeviceSettingsController::Get()->GetConnectedKeyboards());
}

void InputDeviceSettingsProvider::ObserveKeyboardSettings(
    mojo::PendingRemote<mojom::KeyboardSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = keyboard_settings_observers_.Add(std::move(observer));
  keyboard_settings_observers_.Get(id)->OnKeyboardListUpdated(
      InputDeviceSettingsController::Get()->GetConnectedKeyboards());
}

void InputDeviceSettingsProvider::ObserveTouchpadSettings(
    mojo::PendingRemote<mojom::TouchpadSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = touchpad_settings_observers_.Add(std::move(observer));
  touchpad_settings_observers_.Get(id)->OnTouchpadListUpdated(
      InputDeviceSettingsController::Get()->GetConnectedTouchpads());
}

void InputDeviceSettingsProvider::OnKeyboardConnected(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardDisconnected(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadConnected(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadDisconnected(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::NotifyKeyboardsUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  for (const auto& observer : keyboard_settings_observers_) {
    observer->OnKeyboardListUpdated(
        InputDeviceSettingsController::Get()->GetConnectedKeyboards());
  }
}

void InputDeviceSettingsProvider::NotifyTouchpadsUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  for (const auto& observer : touchpad_settings_observers_) {
    observer->OnTouchpadListUpdated(
        InputDeviceSettingsController::Get()->GetConnectedTouchpads());
  }
}

}  // namespace ash::settings
