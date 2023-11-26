// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_

#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
// TODO(b/308717267): Remove this dependency when moving to mojom struct
// definition for Lacros support.
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/installable/installable_data.h"

namespace ash::app_install {

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

// Defines the web dialog used for installing an app.
class AppInstallDialog : public SystemWebDialogDelegate {
 public:
  AppInstallDialog(const AppInstallDialog&) = delete;
  AppInstallDialog& operator=(const AppInstallDialog&) = delete;

  // Creates and shows a new dialog for installing an app. Returns true
  // if a new dialog has been effectively created.
  static bool Show(gfx::NativeWindow parent,
                   ChromeOsAppInstallDialogParams params);

  void OnDialogShown(content::WebUI* webui) override;

 protected:
  explicit AppInstallDialog(mojom::DialogArgsPtr args);
  ~AppInstallDialog() override;
  bool ShouldShowCloseButton() const override;

 private:
  mojom::DialogArgsPtr dialog_args_;
};

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_
