// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_INFO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_INFO_UI_H_

#include "chrome/common/webui_url_constants.h"

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class SensorInfoUI;

// WebUIConfig for chrome://sensor-info.
class SensorInfoUIConfig : public content::DefaultWebUIConfig<SensorInfoUI> {
 public:
  SensorInfoUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISensorInfoHost) {}
};

// The WebUI controller for chrome://sensor-info.
class SensorInfoUI : public content::WebUIController {
 public:
  explicit SensorInfoUI(content::WebUI* web_ui);
  SensorInfoUI(const SensorInfoUI&) = delete;
  SensorInfoUI& operator=(const SensorInfoUI&) = delete;
  ~SensorInfoUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_INFO_UI_H_
