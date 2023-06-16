// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PROVIDER_H_

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
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
  void ObserveKeyboardSettings(
      mojo::PendingRemote<mojom::KeyboardSettingsObserver> observer) override;
  void ObserveTouchpadSettings(
      mojo::PendingRemote<mojom::TouchpadSettingsObserver> observer) override;
  void ObservePointingStickSettings(
      mojo::PendingRemote<mojom::PointingStickSettingsObserver> observer)
      override;
  void ObserveMouseSettings(
      mojo::PendingRemote<mojom::MouseSettingsObserver> observer) override;
  void RestoreDefaultKeyboardRemappings(uint32_t device_id) override;
  void SetKeyboardSettings(uint32_t device_id,
                           ::ash::mojom::KeyboardSettingsPtr settings) override;
  void SetPointingStickSettings(
      uint32_t device_id,
      ::ash::mojom::PointingStickSettingsPtr settings) override;
  void SetMouseSettings(uint32_t device_id,
                        ::ash::mojom::MouseSettingsPtr settings) override;
  void SetTouchpadSettings(uint32_t device_id,
                           ::ash::mojom::TouchpadSettingsPtr settings) override;

  // InputDeviceSettingsController::Observer:
  void OnKeyboardConnected(const ::ash::mojom::Keyboard& keyboard) override;
  void OnKeyboardDisconnected(const ::ash::mojom::Keyboard& keyboard) override;
  void OnKeyboardSettingsUpdated(
      const ::ash::mojom::Keyboard& keyboard) override;
  void OnKeyboardPoliciesUpdated(
      const ::ash::mojom::KeyboardPolicies& keyboard_policies) override;
  void OnTouchpadConnected(const ::ash::mojom::Touchpad& touchpad) override;
  void OnTouchpadDisconnected(const ::ash::mojom::Touchpad& touchpad) override;
  void OnTouchpadSettingsUpdated(
      const ::ash::mojom::Touchpad& touchpad) override;
  void OnPointingStickConnected(
      const ::ash::mojom::PointingStick& pointing_stick) override;
  void OnPointingStickDisconnected(
      const ::ash::mojom::PointingStick& pointing_stick) override;
  void OnPointingStickSettingsUpdated(
      const ::ash::mojom::PointingStick& pointing_stick) override;
  void OnMouseConnected(const ::ash::mojom::Mouse& mouse) override;
  void OnMouseDisconnected(const ::ash::mojom::Mouse& mouse) override;
  void OnMouseSettingsUpdated(const ::ash::mojom::Mouse& mouse) override;
  void OnMousePoliciesUpdated(
      const ::ash::mojom::MousePolicies& mouse_policies) override;

 private:
  void NotifyKeyboardsUpdated();
  void NotifyTouchpadsUpdated();
  void NotifyPointingSticksUpdated();
  void NotifyMiceUpdated();

  mojo::RemoteSet<mojom::KeyboardSettingsObserver> keyboard_settings_observers_;
  mojo::RemoteSet<mojom::TouchpadSettingsObserver> touchpad_settings_observers_;
  mojo::RemoteSet<mojom::PointingStickSettingsObserver>
      pointing_stick_settings_observers_;
  mojo::RemoteSet<mojom::MouseSettingsObserver> mouse_settings_observers_;

  mojo::Receiver<mojom::InputDeviceSettingsProvider> receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PROVIDER_H_
