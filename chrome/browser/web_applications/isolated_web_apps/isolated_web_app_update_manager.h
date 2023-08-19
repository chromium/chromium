// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_waiter.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"

class GURL;
class Profile;

namespace web_package {
class SignedWebBundleId;
}

namespace web_app {

class IsolatedWebAppUrlInfo;
class WebAppProvider;

namespace {
constexpr base::TimeDelta kDefaultUpdateDiscoveryFrequency = base::Hours(5);
}

// The `IsolatedWebAppUpdateManager` is responsible for discovery, download, and
// installation of Isolated Web App updates. Currently, it is only updating
// policy-installed IWAs on ChromeOS.
//
// TODO(crbug.com/1459160): Implement updates for unmanaged IWAs once we have
// designed that process.
//
// TODO(crbug.com/1459161): Consider only executing update discovery tasks when
// the user is not on a metered/paid internet connection.
class IsolatedWebAppUpdateManager : public WebAppInstallManagerObserver {
 public:
  explicit IsolatedWebAppUpdateManager(
      Profile& profile,
      base::TimeDelta update_discovery_frequency =
          kDefaultUpdateDiscoveryFrequency);
  ~IsolatedWebAppUpdateManager() override;

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();

  void Shutdown();

  base::Value AsDebugValue() const;

  void SetEnableAutomaticUpdatesForTesting(bool automatic_updates_enabled);

  // `WebAppInstallManagerObserver`:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppUninstalled(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;

  const base::RepeatingTimer& GetUpdateDiscoveryTimerForTesting() const {
    return update_discovery_timer_;
  }

 private:
  bool IsAnyIWAInstalled();

  void QueueUpdateDiscoveryTasks();

  base::flat_map<web_package::SignedWebBundleId, GURL>
  GetForceInstalledBundleIdToUpdateManifestUrlMap();

  void QueueUpdateDiscoveryTask(const IsolatedWebAppUrlInfo& url_info,
                                const GURL& update_manifest_url);

  void CreateUpdateApplyWaiter(const IsolatedWebAppUrlInfo& url_info);

  // Starts the next update discovery task if (a) no update discovery task is
  // currently running and (b) there is at least one update discovery task in
  // the queue.
  void MaybeStartNextUpdateDiscoveryTask();

  void OnUpdateDiscoveryTaskCompleted(
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status);

  void OnUpdateApplyWaiterFinished(
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive);

  raw_ref<Profile> profile_;
  bool automatic_updates_enabled_;

  raw_ptr<WebAppProvider> provider_ = nullptr;

  bool has_started_ = false;

  base::TimeDelta update_discovery_frequency_;
  base::RepeatingTimer update_discovery_timer_;
  // Update discovery tasks are executed serially one after each other. Only the
  // task at the front of the queue can be running. Once finished, the task will
  // be popped from the queue.
  base::circular_deque<std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask>>
      update_discovery_tasks_;
  base::Value::List update_discovery_results_log_;

  base::flat_map<AppId, std::unique_ptr<IsolatedWebAppUpdateApplyWaiter>>
      update_apply_waiters_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};
  base::WeakPtrFactory<IsolatedWebAppUpdateManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_
