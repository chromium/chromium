// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
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
  ~WebAppInstallManager() override;

  // InstallManager:
  bool CanInstallWebApp(content::WebContents* web_contents) override;
  void LoadWebAppAndCheckInstallability(
      const GURL& web_app_url,
      WebappInstallSource install_source,
      WebAppInstallabilityCheckCallback callback) override;
  void InstallWebAppFromManifest(content::WebContents* contents,
                                 WebappInstallSource install_source,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback) override;
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) override;
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      WebappInstallSource install_source,
      OnceInstallCallback callback) override;
  void InstallWebAppWithParams(content::WebContents* web_contents,
                               const InstallParams& install_params,
                               WebappInstallSource install_source,
                               OnceInstallCallback callback) override;
  // For the old ExtensionSyncService-based system only:
  void InstallWebAppFromSync(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;
  void UpdateWebAppFromInfo(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;
  void Shutdown() override;

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

 private:
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
  // For the new USS-based system only:
  void OnWebAppInstalledAfterSync(const AppId& app_in_sync_install_id,
                                  OnceInstallCallback callback,
                                  const AppId& installed_app_id,
                                  InstallResultCode code);
  void OnWebAppUninstalledAfterSync(std::unique_ptr<WebApp> web_app,
                                    OnceUninstallCallback callback,
                                    bool uninstalled);

  void OnLoadWebAppAndCheckInstallabilityCompleted(
      WebAppInstallTask* task,
      WebAppInstallabilityCheckCallback callback,
      std::unique_ptr<content::WebContents> web_contents,
      const AppId& app_id,
      InstallResultCode code);

  content::WebContents* EnsureWebContentsCreated();
  void OnWebContentsReady(WebAppUrlLoader::Result result);

  DataRetrieverFactory data_retriever_factory_;

  std::unique_ptr<WebAppUrlLoader> url_loader_;

  // All owned tasks.
  using Tasks = base::flat_set<std::unique_ptr<WebAppInstallTask>,
                               base::UniquePtrComparator>;
  Tasks tasks_;

  // Tasks can be queued for sequential completion (to be run one at a time).
  // FIFO. This is a subset of |tasks_|.
  using TaskQueue = base::queue<base::OnceClosure>;
  TaskQueue task_queue_;
  bool is_running_queued_task_ = false;

  // A single WebContents, shared between tasks in |task_queue_|.
  std::unique_ptr<content::WebContents> web_contents_;
  bool web_contents_ready_ = false;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
