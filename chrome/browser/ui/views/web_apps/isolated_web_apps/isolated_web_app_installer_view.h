// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace web_app {

class InstallerScreenView;
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

  virtual void ShowDisabledScreen();

  virtual void ShowGetMetadataScreen();
  virtual void UpdateGetMetadataProgress(double percent, int minutes_remaining);

  virtual void ShowMetadataScreen(
      const SignedWebBundleMetadata& bundle_metadata);

  virtual void ShowInstallScreen(
      const SignedWebBundleMetadata& bundle_metadata);
  virtual void UpdateInstallProgress(double percent, int minutes_remaining);

  virtual void ShowInstallSuccessScreen(
      const SignedWebBundleMetadata& bundle_metadata);

 private:
  void ShowScreen(std::unique_ptr<InstallerScreenView> screen);

  raw_ptr<Delegate> delegate_;
  raw_ptr<InstallerScreenView> screen_;
  bool initialized_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_H_
