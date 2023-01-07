// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMULATOR_DEVICE_EMULATOR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMULATOR_DEVICE_EMULATOR_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace ash {

// The WebUI handler for chrome://device-emulator
class DeviceEmulatorUI : public content::WebUIController {
 public:
  explicit DeviceEmulatorUI(content::WebUI* web_ui);

  DeviceEmulatorUI(const DeviceEmulatorUI&) = delete;
  DeviceEmulatorUI& operator=(const DeviceEmulatorUI&) = delete;

  ~DeviceEmulatorUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMULATOR_DEVICE_EMULATOR_UI_H_
