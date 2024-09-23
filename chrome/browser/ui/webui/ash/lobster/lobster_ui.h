// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_UI_H_

#include "chrome/browser/ui/webui/ash/lobster/lobster_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"

namespace ash {

class LobsterUI : public UntrustedTopChromeWebUIController {
 public:
  explicit LobsterUI(content::WebUI* web_ui);
  ~LobsterUI() override;

  static constexpr std::string GetWebUIName() { return "Lobster"; }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<LobsterPageHandler> page_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_UI_H_
