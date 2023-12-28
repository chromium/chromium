// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_

#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_ui.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

namespace ash::app_install {

const int kIconSize = 32;

// Defines the web dialog used for installing an app.
class AppInstallDialog : public SystemWebDialogDelegate {
 public:
  AppInstallDialog(const AppInstallDialog&) = delete;
  AppInstallDialog& operator=(const AppInstallDialog&) = delete;

  // Creates and returns a new dialog for installing an app.
  static base::WeakPtr<AppInstallDialog> CreateDialog();

  // Displays the dialog.
  void Show(gfx::NativeWindow parent,
            mojom::DialogArgsPtr args,
            std::string expected_app_id,
            base::OnceCallback<void(bool accepted)> dialog_accepted_callback);
  // Callers must call this once the install has finished, passing in the app_id
  // if the installation succeeded or a nullptr if it failed.
  void SetInstallComplete(const std::string* app_id);

  void OnDialogShown(content::WebUI* webui) override;

  // There are some cases where we may have created the dialog, but then never
  // shown it. We need to clean up the dialog in that case.
  void CleanUpDialogIfNotShown();

 private:
  AppInstallDialog();
  ~AppInstallDialog() override;
  bool ShouldShowCloseButton() const override;

  base::WeakPtr<AppInstallDialog> GetWeakPtr();

  mojom::DialogArgsPtr dialog_args_;
  std::string expected_app_id_;

  raw_ptr<AppInstallDialogUI> dialog_ui_ = nullptr;
  base::OnceCallback<void(bool accepted)> dialog_accepted_callback_;

  base::WeakPtrFactory<AppInstallDialog> weak_factory_{this};
};

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_
