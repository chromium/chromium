// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/installability_checker.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace views {
class DialogDelegate;
class View;
class Widget;
}  // namespace views

namespace web_app {

class CallbackDelayer;
class WebAppProvider;

class IsolatedWebAppInstallerViewController
    : public IsolatedWebAppInstallerModel::Observer,
      public IsolatedWebAppInstallerView::Delegate {
 public:
  IsolatedWebAppInstallerViewController(
      Profile* profile,
      WebAppProvider* web_app_provider,
      IsolatedWebAppInstallerModel* model,
      std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> pref_observer);
  ~IsolatedWebAppInstallerViewController() override;

  // Starts the installer state transition. |initialized_callback| will be
  // called once the dialog is initialized and ready for display.
  // |complete_callback| will be called when the dialog is being closed.
  void Start(base::OnceClosure initialized_callback,
             base::OnceClosure completion_callback);

  // Present the installer when it is initialized.
  void Show();

  void FocusWindow();

  // Adds or updates the Isolated Web App Installer window to ChromeOS Shelf.
  void AddOrUpdateWindowToShelf();

  void SetIcon(gfx::ImageSkia icon);

  void SetViewForTesting(IsolatedWebAppInstallerView* view);

  views::Widget* GetWidgetForTesting();

  views::Widget* GetChildWidgetForTesting();

 private:
  friend class IsolatedWebAppInstallerViewUiPixelTest;
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallerViewControllerTest,
                           InstallButtonLaunchesConfirmationDialog);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallerViewControllerTest,
                           ConfirmationDialogMovesToInstallScreen);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallerViewControllerTest,
                           SuccessfulInstallationMovesToSuccessScreen);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallerViewControllerTest,
                           InstallationErrorShowsErrorDialog);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallerViewControllerTest,
                           InstallationErrorRetryRestartsFlow);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallerViewControllerTest,
                           CanLaunchAppAfterInstall);

  struct InstallabilityCheckedVisitor;

  // Handles returning a default value if the controller has been deleted.
  static bool OnAcceptWrapper(
      base::WeakPtr<IsolatedWebAppInstallerViewController> controller);

  bool OnAccept();
  void OnComplete();
  void Close();

  void OnPrefChanged(bool enabled);
  void OnGetMetadataProgressUpdated(double progress);
  void OnInstallabilityChecked(InstallabilityChecker::Result result);
  void OnInstallProgressUpdated(double progress);

  void OnInstallComplete(
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError> result);

  void OnShowMetadataLearnMoreClicked();

  // `IsolatedWebAppInstallerView::Delegate`:
  void OnSettingsLinkClicked() override;
  void OnChildDialogCanceled() override;
  void OnChildDialogAccepted() override;
  void OnChildDialogDestroying() override;

  // `IsolatedWebAppInstallerModel::Observer`:
  void OnStepChanged() override;
  void OnChildDialogChanged() override;

  std::unique_ptr<views::DialogDelegate> CreateDialogDelegate(
      std::unique_ptr<views::View> contents_view);

  std::string instance_id_;
  gfx::NativeWindow window_ = nullptr;
  gfx::ImageSkia icon_ = gfx::ImageSkia();

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<WebAppProvider> web_app_provider_ = nullptr;
  raw_ptr<IsolatedWebAppInstallerModel> model_ = nullptr;
  raw_ptr<IsolatedWebAppInstallerView> view_ = nullptr;
  raw_ptr<views::DialogDelegate> dialog_delegate_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<views::Widget> child_widget_ = nullptr;

  std::unique_ptr<CallbackDelayer> callback_delayer_;
  std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> pref_observer_;
  std::unique_ptr<InstallabilityChecker> installability_checker_;
  bool is_initialized_ = false;

  base::OnceClosure initialized_callback_;
  base::OnceClosure completion_callback_;

  base::WeakPtrFactory<IsolatedWebAppInstallerViewController> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_VIEW_CONTROLLER_H_
