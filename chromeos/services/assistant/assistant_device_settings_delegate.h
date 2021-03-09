// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_DEVICE_SETTINGS_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_DEVICE_SETTINGS_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/mojom/device_settings_delegate.mojom-forward.h"

namespace assistant {
namespace api {
namespace client_op {
class ModifySettingArgs;
class GetDeviceSettingsArgs;
}  // namespace client_op
}  // namespace api
}  // namespace assistant

namespace chromeos {
namespace assistant {

struct DeviceSetting;
class Setting;

// Delegate that handles Assistant actions related to retrieving/modifying
// the device settings, like Bluetooth or WiFi.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantDeviceSettingsDelegate {
 public:
  explicit AssistantDeviceSettingsDelegate(
      chromeos::libassistant::mojom::DeviceSettingsDelegate* mojom_delegate);
  AssistantDeviceSettingsDelegate(AssistantDeviceSettingsDelegate&) = delete;
  AssistantDeviceSettingsDelegate& operator=(AssistantDeviceSettingsDelegate&) =
      delete;
  ~AssistantDeviceSettingsDelegate();

  bool IsSettingSupported(const std::string& setting_id) const;

  void HandleModifyDeviceSetting(
      const ::assistant::api::client_op::ModifySettingArgs& args);

  // Return which of the given device settings are supported or not.
  std::vector<DeviceSetting> GetDeviceSettings(
      const ::assistant::api::client_op::GetDeviceSettingsArgs& args) const;

 private:
  void AddSetting(std::unique_ptr<Setting> setting);

  std::vector<std::unique_ptr<Setting>> settings_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_DEVICE_SETTINGS_DELEGATE_H_
