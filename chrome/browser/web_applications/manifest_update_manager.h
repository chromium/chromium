// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/manifest_update_task.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppUiManager;
class WebAppInstallFinalizer;
class OsIntegrationManager;
class WebAppSyncBridge;

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
class ManifestUpdateManager final : public WebAppInstallManagerObserver {
 public:
  ManifestUpdateManager();
  ~ManifestUpdateManager() override;

  void SetSubsystems(WebAppInstallManager* install_manager,
                     WebAppRegistrar* registrar,
                     WebAppIconManager* icon_manager,
                     WebAppUiManager* ui_manager,
                     WebAppInstallFinalizer* install_finalizer,
                     OsIntegrationManager* os_integration_manager,
                     WebAppSyncBridge* sync_bridge);
  void SetSystemWebAppDelegateMap(
      const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map);

  void Start();
  void Shutdown();

  void MaybeUpdate(const GURL& url,
                   const absl::optional<AppId>& app_id,
                   content::WebContents* web_contents);
  bool IsUpdateConsumed(const AppId& app_id);
  bool IsUpdateTaskPending(const AppId& app_id);

  // WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

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

  void ResetManifestThrottleForTesting(const AppId& app_id);

 private:
  // This class is used to either observe the url loading or web_contents
  // destruction before manifest update tasks can be scheduled. Once any
  // of those events have been fired, observing is stopped.
  class PreUpdateWebContentsObserver;

  // Store information regarding the entire manifest update in different stages.
  // The following steps are followed for the update:
  // 1. The UpdateStage is initialized by passing an observer, who waits till
  // page loading has finished. During the lifetime of the observer,
  // the update_task stays uninitialized.
  // 2. The update_task is initialized as soon as the observer fires a
  // DidFinishLoad and the observer is destructed. This ensures that at any
  // point, either the observer or the update_task exists, but not both. This
  // helps reason about the entire process at different stages of its
  // functionality. This class is owned by the ManifestUpdateManager, and is
  // guaranteed to hold an observer OR an update_task always, but never both.
  struct UpdateStage {
    UpdateStage(const GURL& url,
                std::unique_ptr<PreUpdateWebContentsObserver> observer);
    ~UpdateStage();

    GURL url;
    std::unique_ptr<PreUpdateWebContentsObserver> observer;
    std::unique_ptr<ManifestUpdateTask> update_task;
  };

  void StartUpdateTaskAfterPageLoad(
      const AppId& app_id,
      base::WeakPtr<content::WebContents> web_contents);

  bool MaybeConsumeUpdateCheck(const GURL& origin, const AppId& app_id);
  absl::optional<base::Time> GetLastUpdateCheckTime(const AppId& app_id) const;
  void SetLastUpdateCheckTime(const GURL& origin,
                              const AppId& app_id,
                              base::Time time);
  void OnUpdateStopped(const ManifestUpdateTask& task,
                       ManifestUpdateResult result);
  void NotifyResult(const GURL& url,
                    const absl::optional<AppId>& app_id,
                    ManifestUpdateResult result);

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<WebAppIconManager> icon_manager_ = nullptr;
  raw_ptr<WebAppUiManager> ui_manager_ = nullptr;
  raw_ptr<WebAppInstallFinalizer> install_finalizer_ = nullptr;
  raw_ptr<const ash::SystemWebAppDelegateMap> system_web_apps_delegate_map_ =
      nullptr;
  raw_ptr<OsIntegrationManager> os_integration_manager_ = nullptr;
  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;
  raw_ptr<WebAppInstallManager> install_manager_ = nullptr;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  std::map<AppId, UpdateStage> update_stages_;
  base::flat_map<AppId, base::Time> last_update_check_;

  absl::optional<base::Time> time_override_for_testing_;
  ResultCallback result_callback_for_testing_;

  bool started_ = false;
  bool hang_update_checks_for_testing_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
