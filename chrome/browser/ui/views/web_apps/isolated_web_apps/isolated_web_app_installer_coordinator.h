// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_COORDINATOR_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace web_app {

class IsolatedWebAppInstallerModel;
class IsolatedWebAppInstallerViewController;
class IsolatedWebAppsEnabledPrefObserver;

class IsolatedWebAppInstallerCoordinator {
 public:
  static IsolatedWebAppInstallerCoordinator* CreateAndStart(
      Profile* profile,
      const base::FilePath& bundle_path,
      base::OnceClosure on_closed_callback,
      std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> pref_observer);

  ~IsolatedWebAppInstallerCoordinator();


  void FocusWindow();

  IsolatedWebAppInstallerModel* GetModelForTesting();

  IsolatedWebAppInstallerViewController* GetControllerForTesting();

 private:
  IsolatedWebAppInstallerCoordinator(
      Profile* profile,
      const base::FilePath& bundle_path,
      base::OnceClosure on_closed_callback,
      std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> pref_observer);

  void Start(base::OnceCallback<void(std::optional<webapps::AppId>)> callback);

  void OnDialogClosed(
      base::OnceCallback<void(std::optional<webapps::AppId>)> callback);

  raw_ptr<Profile> profile_;
  base::OnceClosure on_closed_callback_;

  std::unique_ptr<IsolatedWebAppInstallerModel> model_;
  std::unique_ptr<IsolatedWebAppInstallerViewController> controller_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_COORDINATOR_H_
