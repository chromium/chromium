// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class DialogDelegate;
class Widget;
}  // namespace views

namespace web_app {

class SignedWebBundleMetadata;

// Responsible for displaying the contents section of the installation dialog:
//
//   +--------------------+
//   | title     controls |
//   +--------------------+
//   |     *contents*     |
//   +--------------------+
//   |            buttons |
//   +--------------------+
//
// Close/accept buttons and window controls are NOT drawn by this View, nor
// are any nested dialogs that show up during the installation flow. Those are
// all handled by the ViewController.
class IsolatedWebAppInstallerView : public views::View {
  METADATA_HEADER(IsolatedWebAppInstallerView, views::View)

 public:
  static constexpr char kInstallerWidgetName[] = "IsolatedWebAppInstaller";
  static constexpr char kNestedDialogWidgetName[] =
      "IsolatedWebAppInstallerDialog";

  class Delegate {
   public:
    virtual void OnSettingsLinkClicked() = 0;
    virtual void OnChildDialogCanceled() = 0;
    virtual void OnChildDialogAccepted() = 0;
    virtual void OnChildDialogDestroying() = 0;
  };

  // Configures the buttons of the given DialogDelegate.
  static void SetDialogButtons(views::DialogDelegate* dialog_delegate,
                               int close_button_label_id,
                               std::optional<int> accept_button_label_id);

  static std::unique_ptr<IsolatedWebAppInstallerView> Create(
      Delegate* delegate);

  virtual void ShowDisabledScreen() = 0;

  virtual void ShowGetMetadataScreen() = 0;
  virtual void UpdateGetMetadataProgress(double percent) = 0;

  virtual void ShowMetadataScreen(
      const SignedWebBundleMetadata& bundle_metadata) = 0;

  virtual void ShowInstallScreen(
      const SignedWebBundleMetadata& bundle_metadata) = 0;
  virtual void UpdateInstallProgress(double percent) = 0;

  virtual void ShowInstallSuccessScreen(
      const SignedWebBundleMetadata& bundle_metadata) = 0;

  virtual views::Widget* ShowDialog(
      const IsolatedWebAppInstallerModel::Dialog& dialog) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_
