// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/device_settings_host.h"

#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "chromeos/ash/services/assistant/public/cpp/device_actions.h"
#include "chromeos/ash/services/assistant/service_context.h"

namespace ash::assistant {

namespace {

using libassistant::mojom::GetBrightnessResult;
using GetScreenBrightnessLevelCallback = libassistant::mojom::
    DeviceSettingsDelegate::GetScreenBrightnessLevelCallback;

void HandleScreenBrightnessCallback(GetScreenBrightnessLevelCallback callback,
                                    bool success,
                                    double level) {
  if (success) {
    std::move(callback).Run(GetBrightnessResult::New(level));
  } else {
    std::move(callback).Run(nullptr);
  }
}

}  // namespace

DeviceSettingsHost::DeviceSettingsHost(ServiceContext* context)
    : context_(*context) {}

DeviceSettingsHost::~DeviceSettingsHost() = default;

void DeviceSettingsHost::Bind(
    mojo::PendingReceiver<DeviceSettingsDelegate> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void DeviceSettingsHost::Stop() {
  receiver_.reset();
}

void DeviceSettingsHost::GetScreenBrightnessLevel(
    GetScreenBrightnessLevelCallback callback) {
  device_actions().GetScreenBrightnessLevel(
      base::BindOnce(&HandleScreenBrightnessCallback, std::move(callback)));
}

void DeviceSettingsHost::SetBluetoothEnabled(bool enabled) {
  has_setting_changed_ = true;
  device_actions().SetBluetoothEnabled(enabled);
}

void DeviceSettingsHost::SetDoNotDisturbEnabled(bool enabled) {
  has_setting_changed_ = true;
  assistant_notification_controller().SetQuietMode(enabled);
}

void DeviceSettingsHost::SetNightLightEnabled(bool enabled) {
  has_setting_changed_ = true;
  device_actions().SetNightLightEnabled(enabled);
}

void DeviceSettingsHost::SetScreenBrightnessLevel(double level, bool gradual) {
  has_setting_changed_ = true;
  device_actions().SetScreenBrightnessLevel(level, gradual);
}

void DeviceSettingsHost::SetSwitchAccessEnabled(bool enabled) {
  has_setting_changed_ = true;
  device_actions().SetSwitchAccessEnabled(enabled);
}

void DeviceSettingsHost::SetWifiEnabled(bool enabled) {
  has_setting_changed_ = true;
  device_actions().SetWifiEnabled(enabled);
}

void DeviceSettingsHost::reset_has_setting_changed() {
  has_setting_changed_ = false;
}

DeviceActions& DeviceSettingsHost::device_actions() {
  auto* result = context_->device_actions();
  DCHECK(result);
  return *result;
}

AssistantNotificationController&
DeviceSettingsHost::assistant_notification_controller() {
  auto* result = context_->assistant_notification_controller();
  DCHECK(result);
  return *result;
}

}  // namespace ash::assistant
