// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {
class WebUI;
}

namespace ash {

class ArcPowerControlUI;

// WebUIConfig for chrome://arc-power-control
class ArcPowerControlUIConfig
    : public content::DefaultWebUIConfig<ArcPowerControlUI> {
 public:
  ArcPowerControlUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIArcPowerControlHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for ARC power control.
class ArcPowerControlUI : public content::WebUIController {
 public:
  explicit ArcPowerControlUI(content::WebUI* web_ui);

 private:
  ArcPowerControlUI(ArcPowerControlUI const&) = delete;
  ArcPowerControlUI& operator=(ArcPowerControlUI const&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_
