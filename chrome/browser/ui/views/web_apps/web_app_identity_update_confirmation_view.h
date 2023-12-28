// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_IDENTITY_UPDATE_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_IDENTITY_UPDATE_CONFIRMATION_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;
class SkBitmap;

// WebAppIdentityUpdateConfirmationView provides views for showing which parts
// of the app's identity changed so the user can make a determination whether to
// allow the update or uninstall it.
class WebAppIdentityUpdateConfirmationView
    : public views::DialogDelegateView,
      public web_app::WebAppInstallManagerObserver {
  METADATA_HEADER(WebAppIdentityUpdateConfirmationView,
                  views::DialogDelegateView)

 public:
  WebAppIdentityUpdateConfirmationView(
      Profile* profile,
      const std::string& app_id,
      bool title_change,
      bool icon_change,
      const std::u16string& old_title,
      const std::u16string& new_title,
      const SkBitmap& old_icon,
      const SkBitmap& new_icon,
      web_app::AppIdentityDialogCallback callback);
  WebAppIdentityUpdateConfirmationView(
      const WebAppIdentityUpdateConfirmationView&) = delete;
  WebAppIdentityUpdateConfirmationView& operator=(
      const WebAppIdentityUpdateConfirmationView&) = delete;
  ~WebAppIdentityUpdateConfirmationView() override;

 private:
  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstallManagerDestroyed() override;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  bool Cancel() override;

  void OnDialogAccepted();
  void OnWebAppUninstallScheduled(bool uninstall_scheduled);

  const raw_ptr<Profile> profile_;

  // The id of the app whose identity is changing.
  const std::string app_id_;

  // An observer listening for web app uninstalls.
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  // A callback to relay the results of the app identity update dialog.
  web_app::AppIdentityDialogCallback callback_;

  base::WeakPtrFactory<WebAppIdentityUpdateConfirmationView> weak_factory_{
      this};
};

BEGIN_VIEW_BUILDER(,
                   WebAppIdentityUpdateConfirmationView,
                   views::DialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, WebAppIdentityUpdateConfirmationView)

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_IDENTITY_UPDATE_CONFIRMATION_VIEW_H_
