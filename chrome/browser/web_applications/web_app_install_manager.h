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
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"

class Profile;

namespace content {
class WebContents;
}

namespace webapps {
enum class InstallResultCode;
}

namespace web_app {

class WebAppCommandManager;
class WebAppDataRetriever;
class WebAppInstallFinalizer;
class WebAppInstallTask;
class WebAppRegistrar;
class OsIntegrationManager;

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
                     WebAppCommandManager* command_manager,
                     WebAppInstallFinalizer* finalizer);

  // Loads |web_app_url| in a new WebContents and determines whether it has a
  // valid manifest. Calls |callback| with results.
  void LoadWebAppAndCheckManifest(const GURL& web_app_url,
                                  webapps::WebappInstallSource install_source,
                                  WebAppManifestCheckCallback callback);

  void InstallSubApp(const AppId& parent_app_id,
                     const GURL& install_url,
                     const AppId& expected_app_id,
                     WebAppInstallDialogCallback dialog_callback,
                     OnceInstallCallback install_callback);

  // Returns whether the an installation is already running with the
  // same web contents.
  bool IsInstallingForWebContents(
      const content::WebContents* web_contents) const;

  // Returns the number of running web app installations.
  std::size_t GetInstallTaskCountForTesting() const;

  // SyncInstallDelegate:
  void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                               RepeatingInstallCallback callback) override;
  void UninstallFromSync(const std::vector<AppId>& web_apps,
                         RepeatingUninstallCallback callback) override;
  void RetryIncompleteUninstalls(
      const base::flat_set<AppId>& apps_to_uninstall) override;

  virtual void AddObserver(WebAppInstallManagerObserver* observer);
  virtual void RemoveObserver(WebAppInstallManagerObserver* observer);

  virtual void NotifyWebAppInstalled(const AppId& app_id);
  virtual void NotifyWebAppInstalledWithOsHooks(const AppId& app_id);
  virtual void NotifyWebAppUninstalled(const AppId& app_id);
  virtual void NotifyWebAppManifestUpdated(const AppId& app_id,
                                           base::StringPiece old_name);
  virtual void NotifyWebAppWillBeUninstalled(const AppId& app_id);
  virtual void NotifyWebAppInstallManagerDestroyed();

  // Collects icon read/write errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  using ErrorLog = base::Value::ListStorage;
  const ErrorLog* error_log() const { return error_log_.get(); }

  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;
  void SetDataRetrieverFactoryForTesting(
      DataRetrieverFactory data_retriever_factory);

  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> url_loader);
  bool has_web_contents_for_testing() const { return web_contents_ != nullptr; }
  std::set<AppId> GetEnqueuedInstallAppIdsForTesting();

  // TODO(crbug.com/1322974): migrate loggign to WebAppCommandManager after all
  // tasks are migrated to the command system.
  void TakeCommandErrorLog(base::PassKey<WebAppCommandManager>,
                           base::Value log);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppInstallManagerTest,
                           TaskQueueWebContentsReadyRace);

  base::WeakPtr<WebAppInstallManager> GetWeakPtr();

  void EnqueueInstallAppFromSync(
      const AppId& sync_app_id,
      std::unique_ptr<WebAppInstallInfo> install_info,
      OnceInstallCallback callback);
  bool IsAppIdAlreadyEnqueued(const AppId& app_id) const;

  void EnqueueTask(std::unique_ptr<WebAppInstallTask> task,
                   base::OnceClosure start_task);
  void MaybeStartQueuedTask();

  void TakeTaskErrorLog(WebAppInstallTask* task);
  void DeleteTask(WebAppInstallTask* task);
  void OnInstallTaskCompleted(WebAppInstallTask* task,
                              OnceInstallCallback callback,
                              const AppId& app_id,
                              webapps::InstallResultCode code);
  void OnQueuedTaskCompleted(WebAppInstallTask* task,
                             OnceInstallCallback callback,
                             const AppId& app_id,
                             webapps::InstallResultCode code);

  void OnLoadWebAppAndCheckManifestCompleted(
      WebAppInstallTask* task,
      WebAppManifestCheckCallback callback,
      std::unique_ptr<content::WebContents> web_contents,
      const AppId& app_id,
      webapps::InstallResultCode code);

  content::WebContents* EnsureWebContentsCreated();

  // Tasks can be queued for sequential completion (to be run one at a time).
  // FIFO. This is a subset of |tasks_|.
  struct PendingTask {
    PendingTask();
    PendingTask(PendingTask&&) noexcept;
    ~PendingTask();

    raw_ptr<const WebAppInstallTask> task = nullptr;
    base::OnceClosure start;
  };

  void OnWebContentsReadyRunTask(PendingTask pending_task,
                                 WebAppUrlLoader::Result result);

  void MaybeWriteErrorLog();
  void OnWriteErrorLog(Result result);
  void OnReadErrorLog(Result result, base::Value error_log);

  void LogErrorObject(base::Value object);
  void LogErrorObjectAtStage(const char* stage, base::Value object);
  void LogUrlLoaderError(const char* stage,
                         const PendingTask& task,
                         WebAppUrlLoader::Result result);

  DataRetrieverFactory data_retriever_factory_;

  const raw_ptr<Profile> profile_;
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<OsIntegrationManager> os_integration_manager_ = nullptr;
  raw_ptr<WebAppInstallFinalizer> finalizer_ = nullptr;
  raw_ptr<WebAppCommandManager> command_manager_ = nullptr;

  // All owned tasks.
  using Tasks = base::flat_set<std::unique_ptr<WebAppInstallTask>,
                               base::UniquePtrComparator>;
  Tasks tasks_;

  using TaskQueue = base::queue<PendingTask>;
  TaskQueue task_queue_;
  raw_ptr<const WebAppInstallTask> current_queued_task_ = nullptr;

  // A single WebContents, shared between tasks in |task_queue_|.
  std::unique_ptr<content::WebContents> web_contents_;

  bool started_ = false;

  std::unique_ptr<ErrorLog> error_log_;
  bool error_log_updated_ = false;
  bool error_log_writing_in_progress_ = false;

  base::ObserverList<WebAppInstallManagerObserver> observers_;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
