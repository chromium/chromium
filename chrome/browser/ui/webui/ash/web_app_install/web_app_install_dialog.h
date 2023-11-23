// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install.mojom.h"
// TODO(b/308717267): Remove this dependency when moving to mojom struct
// definition for Lacros support.
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/installable/installable_data.h"

namespace ash::web_app_install {

struct ChromeOsAppInstallDialogParams {
  std::optional<SkBitmap> icon_bitmap;
  std::string name;
  GURL url;
  std::string description;
  std::vector<webapps::Screenshot> screenshots;

  ChromeOsAppInstallDialogParams(
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
      std::vector<webapps::Screenshot> screenshots);
  ~ChromeOsAppInstallDialogParams();
};

// Defines the web dialog used for installing a web app.
class WebAppInstallDialog : public SystemWebDialogDelegate {
 public:
  WebAppInstallDialog(const WebAppInstallDialog&) = delete;
  WebAppInstallDialog& operator=(const WebAppInstallDialog&) = delete;

  // Creates and shows a new dialog for installing a web app. Returns true
  // if a new dialog has been effectively created.
  static bool Show(gfx::NativeWindow parent,
                   ChromeOsAppInstallDialogParams params);

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
