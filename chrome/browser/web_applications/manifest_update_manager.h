// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/manifest_update_task.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppUiManager;
class InstallManager;
class OsIntegrationManager;
class SystemWebAppManager;

// Checks for updates to a web app's manifest and triggers a reinstall if the
// current installation is out of date.
//
// Update checks are throttled per app (see MaybeConsumeUpdateCheck()) to avoid
// excessive updating on pathological sites.
//
// Each update check is performed by a |ManifestUpdateTask|, see that class for
// details about what happens during a check.
//
// TODO(crbug.com/926083): Replace MaybeUpdate() with a background check instead
// of being triggered by page loads.
class ManifestUpdateManager final : public AppRegistrarObserver {
 public:
  ManifestUpdateManager();
  ~ManifestUpdateManager() override;

  void SetSubsystems(AppRegistrar* registrar,
                     AppIconManager* icon_manager,
                     WebAppUiManager* ui_manager,
                     InstallManager* install_manager,
                     SystemWebAppManager* system_web_app_manager,
                     OsIntegrationManager* os_integration_manager);
  void Start();
  void Shutdown();

  void MaybeUpdate(const GURL& url,
                   const AppId& app_id,
                   content::WebContents* web_contents);
  bool IsUpdateConsumed(const AppId& app_id);

  // AppRegistrarObserver:
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;

  // |app_id| will be nullptr when |result| is kNoAppInScope.
  using ResultCallback =
      base::OnceCallback<void(const GURL& url, ManifestUpdateResult result)>;
  void SetResultCallbackForTesting(ResultCallback callback);
  void set_time_override_for_testing(base::Time time_override) {
    time_override_for_testing_ = time_override;
  }

  void hang_update_checks_for_testing() {
    hang_update_checks_for_testing_ = true;
  }

 private:
  bool MaybeConsumeUpdateCheck(const GURL& origin, const AppId& app_id);
  base::Optional<base::Time> GetLastUpdateCheckTime(const AppId& app_id) const;
  void SetLastUpdateCheckTime(const GURL& origin,
                              const AppId& app_id,
                              base::Time time);
  void OnUpdateStopped(const ManifestUpdateTask& task,
                       ManifestUpdateResult result);
  void NotifyResult(const GURL& url,
                    const AppId& app_id,
                    ManifestUpdateResult result);

  AppRegistrar* registrar_ = nullptr;
  AppIconManager* icon_manager_ = nullptr;
  WebAppUiManager* ui_manager_ = nullptr;
  InstallManager* install_manager_ = nullptr;
  SystemWebAppManager* system_web_app_manager_ = nullptr;
  OsIntegrationManager* os_integration_manager_ = nullptr;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> registrar_observer_{this};

  base::flat_map<AppId, std::unique_ptr<ManifestUpdateTask>> tasks_;

  base::flat_map<AppId, base::Time> last_update_check_;

  base::Optional<base::Time> time_override_for_testing_;
  ResultCallback result_callback_for_testing_;

  bool started_ = false;
  bool hang_update_checks_for_testing_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
