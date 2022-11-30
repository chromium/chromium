// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

class WebApp;

// WebAppSyncBridge delegates sync-initiated installs and uninstalls using
// this interface.
class SyncInstallDelegate {
 public:
  virtual ~SyncInstallDelegate() = default;

  using RepeatingInstallCallback =
      base::RepeatingCallback<void(const AppId& app_id,
                                   webapps::InstallResultCode code)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const AppId& app_id,
                                   webapps::UninstallResultCode code)>;

  // |web_apps| are already registered and owned by the registrar.
  virtual void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                                       RepeatingInstallCallback callback) = 0;

  // Sync-initiated uninstall.
  // Called before the web apps are removed from the registry by sync. This:
  // * Begins process of uninstalling OS hooks, which initially requires the
  //   registrar to still contain the web app data.
  // * Notifies observers of WebAppWillBeUninstalled.
  // After the app data is fully deleted & os hooks uninstalled:
  // * Notifies observers of WebAppUninstalled.
  // * `callback` is called.
  virtual void UninstallFromSync(const std::vector<AppId>& web_apps,
                                 RepeatingUninstallCallback callback) = 0;

  // Uninstall the given web app ids that were found on startup as partially
  // uninstalled. `apps_to_uninstall` are in the registrar with
  // `is_uninstalling()` set to true. They are expected to be eventually deleted
  // by this call.
  virtual void RetryIncompleteUninstalls(
      const base::flat_set<AppId>& apps_to_uninstall) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
