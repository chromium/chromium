// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_IDENTITY_UPDATE_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_IDENTITY_UPDATE_CONFIRMATION_VIEW_H_

#include <string>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/components/web_app_callback_app_identity.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

class SkBitmap;
class WebAppUninstallDialogViews;

// WebAppIdentityUpdateConfirmationView provides views for showing which parts
// of the app's identity changed so the user can make a determination whether to
// allow the update or uninstall it.
class WebAppIdentityUpdateConfirmationView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(WebAppIdentityUpdateConfirmationView);
  WebAppIdentityUpdateConfirmationView(
      const std::string& app_id,
      bool title_change,
      bool icon_change,
      const std::u16string& old_title,
      const std::u16string& new_title,
      const SkBitmap& old_icon,
      const SkBitmap& new_icon,
      content::WebContents* web_contents,
      web_app::AppIdentityDialogCallback callback);
  WebAppIdentityUpdateConfirmationView(
      const WebAppIdentityUpdateConfirmationView&) = delete;
  WebAppIdentityUpdateConfirmationView& operator=(
      const WebAppIdentityUpdateConfirmationView&) = delete;
  ~WebAppIdentityUpdateConfirmationView() override;

 private:
  // Overridden from views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;

  // Overriden from views::DialogDelegateView:
  bool Cancel() override;

  void OnDialogAccepted();

  // The id of the app whose identity is changing.
  std::string app_id_;

  // A callback to relay the results of the app identity update dialog.
  web_app::AppIdentityDialogCallback callback_;

  // The app uninstall dialog, shown to confirm the uninstallation.
  std::unique_ptr<WebAppUninstallDialogViews> uninstall_dialog_;

  content::WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_IDENTITY_UPDATE_CONFIRMATION_VIEW_H_
