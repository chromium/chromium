// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_REGISTRAR_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_REGISTRAR_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace base {
class Time;
}

namespace web_app {
class WebApp;

class AppRegistrarObserver : public base::CheckedObserver {
 public:
  virtual void OnWebAppInstalled(const AppId& app_id) {}

  // Called when OS hooks installation is finished during Web App installation.
  virtual void OnWebAppInstalledWithOsHooks(const AppId& app_id) {}

  // Called when any field of a web app's local manifest is updated.
  // Note that |old_name| will always be the same as the current name as we
  // don't support name updating yet. See TODO(crbug.com/1088338).
  virtual void OnWebAppManifestUpdated(const AppId& app_id,
                                       base::StringPiece old_name) {}

  // Called before any field of a web app is updated from the sync server.
  // A call site may compare existing WebApp state from the registry against
  // this new WebApp state with sync changes applied.
  virtual void OnWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state) {}

  // Called before a web app is uninstalled, before the uninstallation process
  // begins. |app_id| is still registered in the WebAppRegistrar, and OS hooks
  // have not yet been uninstalled.
  virtual void OnWebAppWillBeUninstalled(const AppId& app_id) {}

  // Called after a web app is uninstalled. |app_id| is no longer registered in
  // the WebAppRegistrar, all OS hooks are uninstalled, and icons have been
  // deleted.
  virtual void OnWebAppUninstalled(const AppId& app_id) {}

  virtual void OnWebAppProfileWillBeDeleted(const AppId& app_id) {}

  virtual void OnAppRegistrarShutdown() {}

  virtual void OnAppRegistrarDestroyed() {}

  // Called after remembering the user choice to always launch an app via
  // a given protocol.
  virtual void OnWebAppProtocolSettingsChanged() {}

  // Called after the app's access to the File Handling API has changed, e.g. by
  // a user selecting "always allow" in the prompt or after a policy update.
  virtual void OnWebAppFileHandlerApprovalStateChanged(const AppId& app_id) {}

  virtual void OnWebAppLocallyInstalledStateChanged(const AppId& app_id,
                                                    bool is_locally_installed) {
  }

  // The disabled status WebApp::chromeos_data().is_disabled of the app backing
  // |app_id| changed.
  virtual void OnWebAppDisabledStateChanged(const AppId& app_id,
                                            bool is_disabled) {}
  virtual void OnWebAppsDisabledModeChanged() {}
  virtual void OnWebAppLastBadgingTimeChanged(const AppId& app_id,
                                              const base::Time& time) {}
  virtual void OnWebAppLastLaunchTimeChanged(const AppId& app_id,
                                             const base::Time& time) {}
  virtual void OnWebAppInstallTimeChanged(const AppId& app_id,
                                          const base::Time& time) {}
  virtual void OnWebAppUserDisplayModeChanged(const AppId& app_id,
                                              DisplayMode user_display_mode) {}
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_REGISTRAR_OBSERVER_H_
