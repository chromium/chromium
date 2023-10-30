// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/installability_checker.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace base {
class Version;
}  // namespace base

namespace views {
class DialogDelegate;
class View;
}  // namespace views

namespace web_app {

class IsolatedWebAppInstallerModel;
class SignedWebBundleMetadata;
class WebAppProvider;

class IsolatedWebAppInstallerViewController
    : public InstallabilityChecker::Delegate,
      public IsolatedWebAppInstallerView::Delegate {
 public:
  IsolatedWebAppInstallerViewController(Profile* profile,
                                        WebAppProvider* web_app_provider,
                                        IsolatedWebAppInstallerModel* model);
  virtual ~IsolatedWebAppInstallerViewController();

  void Start();

  void Show(base::OnceClosure callback);

  void SetViewForTesting(IsolatedWebAppInstallerView* view);

 private:
  // Handles returning a default value if the controller has been deleted.
  static bool OnAcceptWrapper(
      base::WeakPtr<IsolatedWebAppInstallerViewController> controller);

  bool OnAccept();
  void OnComplete();
  void Close();

  // `InstallabilityChecker::Delegate`:
  void OnProfileShutdown() override;
  void OnBundleInvalid(const std::string& error) override;
  void OnBundleInstallable(const SignedWebBundleMetadata& metadata) override;
  void OnBundleUpdatable(const SignedWebBundleMetadata& metadata,
                         const base::Version& installed_version) override;
  void OnBundleOutdated(const SignedWebBundleMetadata& metadata,
                        const base::Version& installed_version) override;

  // `IsolatedWebAppInstallerView::Delegate`:
  void OnSettingsLinkClicked() override;

  // Updates the View to reflect the current state of the model.
  void OnModelChanged();

  void SetButtons(int close_button_label_id,
                  absl::optional<int> accept_button_label_id);

  std::unique_ptr<views::DialogDelegate> CreateDialogDelegate(
      std::unique_ptr<views::View> contents_view);

  raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> web_app_provider_;
  raw_ptr<IsolatedWebAppInstallerModel> model_;
  raw_ptr<IsolatedWebAppInstallerView> view_;
  raw_ptr<views::DialogDelegate> dialog_delegate_;

  std::unique_ptr<InstallabilityChecker> installability_checker_;

  base::OnceClosure callback_;
  base::WeakPtrFactory<IsolatedWebAppInstallerViewController> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_
