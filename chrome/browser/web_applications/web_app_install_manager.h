// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;
class WebAppInstallFinalizer;
class OsIntegrationManager;
class WebAppDataRetriever;
class WebAppInstallTask;
class WebAppRegistrar;

// TODO(loyso): Unify the API and merge similar InstallWebAppZZZZ functions.
class WebAppInstallManager final : public SyncInstallDelegate {
 public:
  explicit WebAppInstallManager(Profile* profile);
  WebAppInstallManager(const WebAppInstallManager&) = delete;
  WebAppInstallManager& operator=(const WebAppInstallManager&) = delete;
  ~WebAppInstallManager() override;

  void Start();
  void Shutdown();

  void SetSubsystems(WebAppRegistrar* registrar,
                     OsIntegrationManager* os_integration_manager,
                     WebAppInstallFinalizer* finalizer);

  // Loads |web_app_url| in a new WebContents and determines whether it has a
  // valid manifest. Calls |callback| with results.
  void LoadWebAppAndCheckManifest(const GURL& web_app_url,
                                  webapps::WebappInstallSource install_source,
                                  WebAppManifestCheckCallback callback);

  // Checks a WebApp installability (service worker check optional), retrieves
  // manifest and icons and then performs the actual installation.
  void InstallWebAppFromManifest(content::WebContents* contents,
                                 bool bypass_service_worker_check,
                                 webapps::WebappInstallSource install_source,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback);

  // Infers WebApp info from the blink renderer process and then retrieves a
  // manifest in a way similar to |InstallWebAppFromManifest|. If the manifest
  // is incomplete or missing, the inferred info is used. |force_shortcut_app|
  // forces the creation of a shortcut app instead of a PWA even if installation
  // is available.
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* contents,
      bool force_shortcut_app,
      webapps::WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback);

  // Starts a background web app installation process for a given
  // |web_contents|.
  void InstallWebAppWithParams(content::WebContents* web_contents,
                               const WebAppInstallParams& install_params,
                               webapps::WebappInstallSource install_source,
                               OnceInstallCallback callback);

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // This doesn't fetch a manifest and doesn't perform all required steps for
  // External installed apps: use |ExternallyManagedAppManager::Install|
  // instead.
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      webapps::WebappInstallSource install_source,
      OnceInstallCallback callback);

  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      const absl::optional<WebAppInstallParams>& install_params,
      webapps::WebappInstallSource install_source,
      OnceInstallCallback callback);

  // Reinstall an existing web app. If |redownload_app_icons| is true, will
  // redownload app icons and update them on disk. Otherwise, the icons in
  // |web_application_info.bitmap_icons| will be used and saved to disk.
  void UpdateWebAppFromInfo(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool redownload_app_icons,
      OnceInstallCallback callback);

  // For the new USS-based system only. SyncInstallDelegate:
  void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                               RepeatingInstallCallback callback) override;
  void UninstallFromSyncBeforeRegistryUpdate(
      std::vector<AppId> web_apps) override;
  void UninstallFromSyncAfterRegistryUpdate(
      std::vector<std::unique_ptr<WebApp>> web_apps,
      RepeatingUninstallCallback callback) override;

  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;
  void SetDataRetrieverFactoryForTesting(
      DataRetrieverFactory data_retriever_factory);

  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> url_loader);
  bool has_web_contents_for_testing() const { return web_contents_ != nullptr; }
  std::set<AppId> GetEnqueuedInstallAppIdsForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppInstallManagerTest,
                           TaskQueueWebContentsReadyRace);

  void EnqueueInstallAppFromSync(
      const AppId& sync_app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback);
  bool IsAppIdAlreadyEnqueued(const AppId& app_id) const;

  // On failure will attempt a fallback install only loading icon URLs.
  void LoadAndInstallWebAppFromManifestWithFallbackCompleted_ForAppSync(
      const AppId& sync_app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback,
      const AppId& web_app_id,
      InstallResultCode code);

  void EnqueueTask(std::unique_ptr<WebAppInstallTask> task,
                   base::OnceClosure start_task);
  void MaybeStartQueuedTask();

  void DeleteTask(WebAppInstallTask* task);
  void OnInstallTaskCompleted(WebAppInstallTask* task,
                              OnceInstallCallback callback,
                              const AppId& app_id,
                              InstallResultCode code);
  void OnQueuedTaskCompleted(WebAppInstallTask* task,
                             OnceInstallCallback callback,
                             const AppId& app_id,
                             InstallResultCode code);

  void OnLoadWebAppAndCheckManifestCompleted(
      WebAppInstallTask* task,
      WebAppManifestCheckCallback callback,
      std::unique_ptr<content::WebContents> web_contents,
      const AppId& app_id,
      InstallResultCode code);

  content::WebContents* EnsureWebContentsCreated();

  // Tasks can be queued for sequential completion (to be run one at a time).
  // FIFO. This is a subset of |tasks_|.
  struct PendingTask {
    PendingTask();
    PendingTask(PendingTask&&);
    ~PendingTask();

    const WebAppInstallTask* task = nullptr;
    base::OnceClosure start;
  };

  void OnWebContentsReadyRunTask(PendingTask pending_task,
                                 WebAppUrlLoader::Result result);

  DataRetrieverFactory data_retriever_factory_;

  Profile* const profile_;
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  WebAppRegistrar* registrar_ = nullptr;
  OsIntegrationManager* os_integration_manager_ = nullptr;
  WebAppInstallFinalizer* finalizer_ = nullptr;

  bool disable_web_app_sync_install_for_testing_ = false;

  // All owned tasks.
  using Tasks = base::flat_set<std::unique_ptr<WebAppInstallTask>,
                               base::UniquePtrComparator>;
  Tasks tasks_;

  using TaskQueue = base::queue<PendingTask>;
  TaskQueue task_queue_;
  const WebAppInstallTask* current_queued_task_ = nullptr;

  // A single WebContents, shared between tasks in |task_queue_|.
  std::unique_ptr<content::WebContents> web_contents_;

  bool started_ = false;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
