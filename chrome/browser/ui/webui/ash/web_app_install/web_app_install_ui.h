// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash::web_app_install {

// The WebUI for chrome://web-app-install-dialog, used for confirming web app
// installation.
class WebAppInstallDialogUI : public ui::WebDialogUI {
 public:
  explicit WebAppInstallDialogUI(content::WebUI* web_ui);
  WebAppInstallDialogUI(const WebAppInstallDialogUI&) = delete;
  WebAppInstallDialogUI& operator=(const WebAppInstallDialogUI&) = delete;
  ~WebAppInstallDialogUI() override;
};

// WebUIConfig for chrome://web-app-install-dialog
class WebAppInstallDialogUIConfig
    : public content::DefaultWebUIConfig<WebAppInstallDialogUI> {
 public:
  WebAppInstallDialogUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash::web_app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_UI_H_
