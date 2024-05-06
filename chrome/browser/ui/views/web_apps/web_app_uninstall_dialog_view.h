// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_uninstall_dialog_user_options.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class Profile;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace views {
class Checkbox;
}

// The dialog's view, owned by the views framework.
class WebAppUninstallDialogDelegateView
    : public views::DialogDelegateView,
      public web_app::WebAppInstallManagerObserver {
  METADATA_HEADER(WebAppUninstallDialogDelegateView, views::DialogDelegateView)

 public:
  // Constructor for view component of dialog.
  WebAppUninstallDialogDelegateView(
      Profile* profile,
      webapps::AppId app_id,
      webapps::WebappUninstallSource uninstall_source,
      std::map<web_app::SquareSizePx, SkBitmap> icon_bitmaps,
      web_app::UninstallDialogCallback uninstall_choice_callback);
  WebAppUninstallDialogDelegateView(const WebAppUninstallDialogDelegateView&) =
      delete;
  WebAppUninstallDialogDelegateView& operator=(
      const WebAppUninstallDialogDelegateView&) = delete;
  ~WebAppUninstallDialogDelegateView() override;

  void ProcessAutoConfirmValue();

 private:
  // views::DialogDelegateView:
  ui::ImageModel GetWindowIcon() override;

  // Uninstalls the web app.
  void Uninstall(bool clear_site_data);

  void OnDialogAccepted();
  void OnDialogCanceled();

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  raw_ptr<views::Checkbox> checkbox_ = nullptr;
  gfx::ImageSkia image_;

  // The web app we are showing the dialog for.
  const webapps::AppId app_id_;

  const raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;
  base::WeakPtr<web_app::WebAppProvider> provider_;
  web_app::UninstallDialogCallback uninstall_choice_callback_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  webapps::WebappUninstallSource uninstall_source_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_UNINSTALL_DIALOG_VIEW_H_
