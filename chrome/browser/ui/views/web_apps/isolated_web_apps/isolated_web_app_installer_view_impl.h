// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ProgressBar;
}  // namespace views

namespace web_app {

class InstallerDialogView;
class SignedWebBundleMetadata;

class IsolatedWebAppInstallerViewImpl : public IsolatedWebAppInstallerView {
 public:
  METADATA_HEADER(IsolatedWebAppInstallerViewImpl);

  explicit IsolatedWebAppInstallerViewImpl(Delegate* delegate);
  ~IsolatedWebAppInstallerViewImpl() override;

  void ShowDisabledScreen() override;

  void ShowGetMetadataScreen() override;
  void UpdateGetMetadataProgress(double percent) override;

  void ShowMetadataScreen(
      const SignedWebBundleMetadata& bundle_metadata) override;

  void ShowInstallScreen(
      const SignedWebBundleMetadata& bundle_metadata) override;
  void UpdateInstallProgress(double percent) override;

  void ShowInstallSuccessScreen(
      const SignedWebBundleMetadata& bundle_metadata) override;

  void ShowDialog(const IsolatedWebAppInstallerModel::DialogContent&
                      dialog_content) override;

 private:
  void ShowScreen(std::unique_ptr<InstallerDialogView> screen,
                  views::ProgressBar* progress_bar = nullptr);

  raw_ptr<Delegate> delegate_;
  raw_ptr<InstallerDialogView> dialog_view_;
  raw_ptr<views::ProgressBar> progress_bar_;
  bool initialized_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_IMPL_H_
