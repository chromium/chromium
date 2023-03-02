// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PROVIDER_H_

#include "ash/public/cpp/input_device_settings_controller.h"
#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::settings {

class InputDeviceSettingsProvider
    : public mojom::InputDeviceSettingsProvider,
      public InputDeviceSettingsController::Observer {
 public:
  InputDeviceSettingsProvider();
  InputDeviceSettingsProvider(const InputDeviceSettingsProvider& other) =
      delete;
  InputDeviceSettingsProvider& operator=(
      const InputDeviceSettingsProvider& other) = delete;

  ~InputDeviceSettingsProvider() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::InputDeviceSettingsProvider> receiver);

  // mojom::InputDeviceSettingsProvider:
  void GetConnectedKeyboards(GetConnectedKeyboardsCallback callback) override;
  void ObserveKeyboardSettings(
      mojo::PendingRemote<mojom::KeyboardSettingsObserver> observer) override;

  // InputDeviceSettingsController::Observer:
  void OnKeyboardConnected(const ::ash::mojom::Keyboard& keyboard) override;
  void OnKeyboardDisconnected(const ::ash::mojom::Keyboard& keyboard) override;

 private:
  void NotifyKeyboardsUpdated();

  mojo::RemoteSet<mojom::KeyboardSettingsObserver> keyboard_settings_observers_;

  mojo::Receiver<mojom::InputDeviceSettingsProvider> receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PROVIDER_H_
