// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;
class WebAppDataRetriever;
class WebAppInstallTask;

class WebAppInstallManager final : public InstallManager,
                                   public SyncInstallDelegate {
 public:
  explicit WebAppInstallManager(Profile* profile);
  WebAppInstallManager(const WebAppInstallManager&) = delete;
  WebAppInstallManager& operator=(const WebAppInstallManager&) = delete;
  ~WebAppInstallManager() override;

  void Start();
  void Shutdown();

  // InstallManager:
  void LoadWebAppAndCheckManifest(
      const GURL& web_app_url,
      webapps::WebappInstallSource install_source,
      WebAppManifestCheckCallback callback) override;
  void InstallWebAppFromManifest(content::WebContents* contents,
                                 bool bypass_service_worker_check,
                                 webapps::WebappInstallSource install_source,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback) override;
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* contents,
      bool force_shortcut_app,
      webapps::WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) override;

  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      webapps::WebappInstallSource install_source,
      OnceInstallCallback callback) override;

  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      const base::Optional<InstallParams>& install_params,
      webapps::WebappInstallSource install_source,
      OnceInstallCallback callback) override;
  void InstallWebAppWithParams(content::WebContents* web_contents,
                               const InstallParams& install_params,
                               webapps::WebappInstallSource install_source,
                               OnceInstallCallback callback) override;
  void InstallBookmarkAppFromSync(
      const AppId& bookmark_app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;
  void UpdateWebAppFromInfo(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool redownload_app_icons,
      OnceInstallCallback callback) override;

  // For the new USS-based system only. SyncInstallDelegate:
  void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                               RepeatingInstallCallback callback) override;
  void UninstallWebAppsAfterSync(std::vector<std::unique_ptr<WebApp>> web_apps,
                                 RepeatingUninstallCallback callback) override;

  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;
  void SetDataRetrieverFactoryForTesting(
      DataRetrieverFactory data_retriever_factory);

  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> url_loader);
  bool has_web_contents_for_testing() const { return web_contents_ != nullptr; }
  size_t tasks_size_for_testing() const { return tasks_.size(); }
  std::set<AppId> GetEnqueuedInstallAppIdsForTesting() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppInstallManagerTest,
                           TaskQueueWebContentsReadyRace);

  void MaybeEnqueuePendingAppSyncInstalls();
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

  std::unique_ptr<WebAppUrlLoader> url_loader_;

  // All owned tasks.
  using Tasks = base::flat_set<std::unique_ptr<WebAppInstallTask>,
                               base::UniquePtrComparator>;
  Tasks tasks_;

  using TaskQueue = base::queue<PendingTask>;
  TaskQueue task_queue_;
  const WebAppInstallTask* current_queued_task_ = nullptr;

  struct AppSyncInstallRequest {
    AppSyncInstallRequest();
    AppSyncInstallRequest(AppSyncInstallRequest&&);
    ~AppSyncInstallRequest();

    AppId sync_app_id;
    std::unique_ptr<WebApplicationInfo> web_application_info;
    OnceInstallCallback callback;
  };
  std::vector<AppSyncInstallRequest> pending_app_sync_installs_;

  // A single WebContents, shared between tasks in |task_queue_|.
  std::unique_ptr<content::WebContents> web_contents_;

  bool started_ = false;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
