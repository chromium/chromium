// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_COORDINATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace web_app {

class IsolatedWebAppInstallerModel;
class IsolatedWebAppInstallerViewController;

class IsolatedWebAppInstallerCoordinator {
 public:
  IsolatedWebAppInstallerCoordinator(Profile* profile,
                                     const base::FilePath& bundle_path);

  ~IsolatedWebAppInstallerCoordinator();

  void Show(base::OnceCallback<void(absl::optional<webapps::AppId>)> callback);

 private:
  void OnDialogClosed(
      base::OnceCallback<void(absl::optional<webapps::AppId>)> callback);

  std::unique_ptr<IsolatedWebAppInstallerModel> model_;
  std::unique_ptr<IsolatedWebAppInstallerViewController> controller_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_COORDINATOR_H_
