// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

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
class IsolatedWebAppInstallerView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(IsolatedWebAppInstallerView);

  class Delegate {
   public:
    virtual void OnSettingsLinkClicked() = 0;
  };

  explicit IsolatedWebAppInstallerView(Delegate* delegate);
  ~IsolatedWebAppInstallerView() override;

  void ShowDisabledScreen();

  void ShowGetMetadataScreen();
  void UpdateGetMetadataProgress(double percent);

  void ShowMetadataScreen(const SignedWebBundleMetadata& bundle_metadata);

  void ShowInstallScreen(const SignedWebBundleMetadata& bundle_metadata);
  void UpdateInstallProgress(double percent);

  void ShowInstallSuccessScreen(const SignedWebBundleMetadata& bundle_metadata);

 private:
  void SetActiveView(std::unique_ptr<View> view);

  raw_ptr<Delegate> delegate_;
  raw_ptr<View> active_view_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_
