// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

namespace chromeos {

// WebUI controller for ARC power control.
class ArcPowerControlUI : public content::WebUIController {
 public:
  explicit ArcPowerControlUI(content::WebUI* web_ui);

 private:
  ArcPowerControlUI(ArcPowerControlUI const&) = delete;
  ArcPowerControlUI& operator=(ArcPowerControlUI const&) = delete;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_
