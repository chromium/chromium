// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_

#include <optional>

#include "chrome/browser/apps/almanac_api_client/almanac_icon_cache.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog_args.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_ui.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "ui/views/native_window_tracker.h"

namespace apps {
class AlmanacAppIconLoader;
}

namespace ash::app_install {

const int kIconSize = 32;

// Defines the web dialog used for installing an app.
class AppInstallDialog : public SystemWebDialogDelegate {
 public:
  // Creates and returns a new dialog for installing an app.
  static base::WeakPtr<AppInstallDialog> CreateDialog();

  AppInstallDialog(const AppInstallDialog&) = delete;
  AppInstallDialog& operator=(const AppInstallDialog&) = delete;

  // Displays the dialog with app info.
  void ShowApp(
      Profile* profile,
      gfx::NativeWindow parent,
      apps::PackageId package_id,
      std::string app_name,
      GURL app_url,
      std::string app_description,
      std::optional<apps::AppInstallIcon> icon,
      std::vector<mojom::ScreenshotPtr> screenshots,
      base::OnceCallback<void(bool accepted)> dialog_accepted_callback);

  // Displays the dialog with an error message that the app can't be found.
  void ShowNoAppError(gfx::NativeWindow parent);

  // Displays the dialog with an error message that the connection failed.
  void ShowConnectionError(gfx::NativeWindow parent,
                           base::OnceClosure try_again_callback);

  // Callers must call one of SetInstallSucceeded or SetInstallFailed once the
  // install has finished, passing in the app_id if the installation succeeded
  // or a callback to retry the install if it failed.
  void SetInstallSucceeded();
  void SetInstallFailed(base::OnceCallback<void(bool accepted)> retry_callback);

  // There are some cases where we may have created the dialog, but then never
  // shown it. We need to clean up the dialog in that case.
  void CleanUpDialogIfNotShown();

  // SystemWebDialogDelegate:
  void OnDialogShown(content::WebUI* webui) override;
  bool ShouldShowCloseButton() const override;
  void GetDialogSize(gfx::Size* size) const override;

 private:
  AppInstallDialog();
  ~AppInstallDialog() override;

  void OnAppIconLoaded(apps::IconValuePtr icon_value);

  void Show(gfx::NativeWindow parent, AppInstallDialogArgs dialog_args);

  base::WeakPtr<AppInstallDialog> GetWeakPtr();

  std::optional<AppInstallDialogArgs> dialog_args_;
  int dialog_height_ = 0;
  raw_ptr<AppInstallDialogUI> dialog_ui_ = nullptr;

  // Temporary variables for ShowApp().
  base::WeakPtr<Profile> profile_;
  gfx::NativeWindow parent_;
  std::unique_ptr<views::NativeWindowTracker> parent_window_tracker_;
  AppInfoArgs app_info_args_;
  std::unique_ptr<apps::AlmanacAppIconLoader> icon_loader_;

  base::WeakPtrFactory<AppInstallDialog> weak_factory_{this};
};

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_H_
