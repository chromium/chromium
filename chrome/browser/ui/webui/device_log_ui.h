// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVICE_LOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEVICE_LOG_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

class DeviceLogUI : public content::WebUIController {
 public:
  explicit DeviceLogUI(content::WebUI* web_ui);

  DeviceLogUI(const DeviceLogUI&) = delete;
  DeviceLogUI& operator=(const DeviceLogUI&) = delete;

  ~DeviceLogUI() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_DEVICE_LOG_UI_H_
