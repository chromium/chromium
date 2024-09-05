// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_POWER_UI_POWER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_POWER_UI_POWER_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class PowerUI;

// WebUIConfig for chrome://power
class PowerUIConfig : public content::DefaultWebUIConfig<PowerUI> {
 public:
  PowerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPowerHost) {}
};
class PowerUI : public content::WebUIController {
 public:
  explicit PowerUI(content::WebUI* web_ui);

  PowerUI(const PowerUI&) = delete;
  PowerUI& operator=(const PowerUI&) = delete;

  ~PowerUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_POWER_UI_POWER_UI_H_
