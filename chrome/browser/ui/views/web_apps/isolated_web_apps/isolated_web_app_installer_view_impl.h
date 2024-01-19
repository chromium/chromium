// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
class DialogModelLabel;
class ImageModel;
}  // namespace ui

namespace web_app {

class DisabledView;
class GetMetadataView;
class InstallView;
class InstallSuccessView;
class ShowMetadataView;
class SignedWebBundleMetadata;

class IsolatedWebAppInstallerViewImpl : public IsolatedWebAppInstallerView {
  METADATA_HEADER(IsolatedWebAppInstallerViewImpl, IsolatedWebAppInstallerView)

 public:
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

  void ShowDialog(const IsolatedWebAppInstallerModel::Dialog& dialog) override;

  // `views::View`:
  gfx::Size GetMaximumSize() const override;

 private:
  template <class T, class... Args>
  T* MakeAndAddChildView(Args&&... args) {
    return AddChildView(std::make_unique<T>(std::forward<Args>(args)...));
  }

  void ShowChildDialog(int title,
                       const ui::DialogModelLabel& subtitle,
                       const ui::ImageModel& icon,
                       std::optional<int> ok_label);

  void OnChildDialogAccepted();
  void OnChildDialogCanceled();

  void ShowChildView(views::View* view);

  raw_ptr<IsolatedWebAppInstallerView::Delegate> delegate_;

  raw_ptr<DisabledView> disabled_view_;
  raw_ptr<GetMetadataView> get_metadata_view_;
  raw_ptr<ShowMetadataView> show_metadata_view_;
  raw_ptr<InstallView> install_view_;
  raw_ptr<InstallSuccessView> install_success_view_;

  bool dialog_visible_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_IMPL_H_
