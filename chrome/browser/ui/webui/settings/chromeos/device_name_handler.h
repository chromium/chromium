// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_NAME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_NAME_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/device_name/device_name_store.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace chromeos {
namespace settings {

// DeviceNameHandler handles calls from WebUI JS related to getting and setting
// the device name.
class DeviceNameHandler : public ::settings::SettingsPageUIHandler,
                          public DeviceNameStore::Observer {
 public:
  DeviceNameHandler();

  DeviceNameHandler(const DeviceNameHandler&) = delete;
  DeviceNameHandler& operator=(const DeviceNameHandler&) = delete;

  ~DeviceNameHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 protected:
  void HandleAttemptSetDeviceName(const base::Value::List& args);
  void HandleNotifyReadyForDeviceName(const base::Value::List& args);

 private:
  friend class TestDeviceNameHandler;

  // DeviceNameStore::Observer:
  void OnDeviceNameMetadataChanged() override;

  explicit DeviceNameHandler(DeviceNameStore* device_name_store);

  base::Value::Dict GetDeviceNameMetadata() const;

  DeviceNameStore* device_name_store_;

  base::ScopedObservation<DeviceNameStore, DeviceNameStore::Observer>
      observation_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_NAME_HANDLER_H_
