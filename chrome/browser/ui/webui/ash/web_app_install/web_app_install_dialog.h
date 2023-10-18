// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install.mojom.h"

namespace ash::web_app_install {

// Defines the web dialog used for installing a web app.
class WebAppInstallDialog : public SystemWebDialogDelegate {
 public:
  WebAppInstallDialog(const WebAppInstallDialog&) = delete;
  WebAppInstallDialog& operator=(const WebAppInstallDialog&) = delete;

  // Creates and shows a new dialog for installing a web app. Returns true
  // if a new dialog has been effectively created.
  static bool Show();

  void OnDialogShown(content::WebUI* webui) override;

 protected:
  explicit WebAppInstallDialog(mojom::DialogArgsPtr args);
  ~WebAppInstallDialog() override;
  bool ShouldShowCloseButton() const override;

 private:
  mojom::DialogArgsPtr dialog_args_;
};

}  // namespace ash::web_app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_DIALOG_H_
