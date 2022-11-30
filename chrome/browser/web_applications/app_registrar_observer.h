// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_REGISTRAR_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_REGISTRAR_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace base {
class Time;
}

namespace web_app {
class WebApp;

class AppRegistrarObserver : public base::CheckedObserver {
 public:
  // Called before any field of a web app is updated from the sync server.
  // A call site may compare existing WebApp state from the registry against
  // this new WebApp state with sync changes applied.
  virtual void OnWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state) {}

  virtual void OnWebAppProfileWillBeDeleted(const AppId& app_id) {}

  virtual void OnAppRegistrarDestroyed() = 0;

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
  virtual void OnWebAppUserDisplayModeChanged(
      const AppId& app_id,
      UserDisplayMode user_display_mode) {}
  virtual void OnWebAppRunOnOsLoginModeChanged(
      const AppId& app_id,
      RunOnOsLoginMode run_on_os_login_mode) {}

  // Called after the WebAppSettings policy has been updated. If a policy is set
  // this event is also fired during browser startup after the policy has been
  // applied.
  virtual void OnWebAppSettingsPolicyChanged() {}

  virtual void OnAlwaysShowToolbarInFullscreenChanged(const AppId& app_id,
                                                      bool show) {}
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_REGISTRAR_OBSERVER_H_
