// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"

namespace ash::settings {

InputDeviceSettingsProvider::InputDeviceSettingsProvider(
    InputDeviceSettingsController* controller)
    : controller_(controller) {}
InputDeviceSettingsProvider::~InputDeviceSettingsProvider() = default;

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
  std::move(callback).Run(controller_->GetConnectedKeyboards());
}

}  // namespace ash::settings
