// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

class WebApp;

// WebAppSyncBridge delegates sync-initiated installs and uninstalls using
// this interface.
class SyncInstallDelegate {
 public:
  virtual ~SyncInstallDelegate() = default;

  using RepeatingInstallCallback =
      base::RepeatingCallback<void(const AppId& app_id,
                                   InstallResultCode code)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const AppId& app_id, bool uninstalled)>;

  // |web_apps| are already registered and owned by the registrar.
  virtual void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                                       RepeatingInstallCallback callback) = 0;

  // Sync-initiated uninstall.
  // Called before the web apps are removed from the registry. Begins process of
  // uninstalling OS hooks, which initially requires the registrar to still
  // contain the web app data. Also notify observers of WebAppWillBeUninstalled.
  // TODO(dmurph): After migration to WebApp* from the registry, this could
  // potentially just be done in one step, after removal from registry, as os
  // hooks information could be passed.
  virtual void UninstallFromSyncBeforeRegistryUpdate(
      std::vector<AppId> web_apps) = 0;
  // Delete non-database app data from disk (icon .png files). |app_id| must be
  // unregistered. Observers are notified of WebAppUninstalled and the
  // |callback| is called after the app data is fully deleted & os hooks
  // uninstalled.
  virtual void UninstallFromSyncAfterRegistryUpdate(
      std::vector<std::unique_ptr<WebApp>> web_apps,
      RepeatingUninstallCallback callback) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
