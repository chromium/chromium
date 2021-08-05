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
#include "base/memory/checked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;
class InstallFinalizer;
class OsIntegrationManager;
class WebAppDataRetriever;
class WebAppInstallTask;
class WebAppRegistrar;

// TODO(loyso): Unify the API and merge similar InstallWebAppZZZZ functions.
class WebAppInstallManager final : public SyncInstallDelegate {
 public:
  // |app_id| may be empty on failure.
  using OnceInstallCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;
  using OnceUninstallCallback =
      base::OnceCallback<void(const AppId& app_id, bool uninstalled)>;

  // Callback used to indicate whether a user has accepted the installation of a
  // web app.
  using WebAppInstallationAcceptanceCallback =
      base::OnceCallback<void(bool user_accepted,
                              std::unique_ptr<WebApplicationInfo>)>;

  // Callback to show the WebApp installation confirmation bubble in UI.
  // |web_app_info| is the WebApplicationInfo to be installed.
  using WebAppInstallDialogCallback = base::OnceCallback<void(
      content::WebContents* initiator_web_contents,
      std::unique_ptr<WebApplicationInfo> web_app_info,
      ForInstallableSite for_installable_site,
      WebAppInstallationAcceptanceCallback acceptance_callback)>;

  enum class InstallableCheckResult {
    kNotInstallable,
    kInstallable,
    kAlreadyInstalled,
  };
  // Callback with the result of manifest check.
  // |web_contents| owns the WebContents that was used to check for a manifest.
  // |app_id| will be present iff already installed.
  using WebAppManifestCheckCallback = base::OnceCallback<void(
      std::unique_ptr<content::WebContents> web_contents,
      InstallableCheckResult result,
      absl::optional<AppId> app_id)>;

  explicit WebAppInstallManager(Profile* profile);
  WebAppInstallManager(const WebAppInstallManager&) = delete;
  WebAppInstallManager& operator=(const WebAppInstallManager&) = delete;
  ~WebAppInstallManager() override;

  void Start();
  void Shutdown();

  void SetSubsystems(WebAppRegistrar* registrar,
                     OsIntegrationManager* os_integration_manager,
                     InstallFinalizer* finalizer);

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

  // See related ExternalInstallOptions struct and
  // ConvertExternalInstallOptionsToParams function.
  struct InstallParams {
    InstallParams();
    ~InstallParams();
    InstallParams(const InstallParams&);

    DisplayMode user_display_mode = DisplayMode::kUndefined;

    // URL to be used as start_url if manifest is unavailable.
    GURL fallback_start_url;

    // // Setting this field will force the webapp to have a manifest id, which
    // will result in a different AppId than if it isn't set. Currently here
    // to support forwards compatibility with future sync entities..
    absl::optional<std::string> override_manifest_id;

    // App name to be used if manifest is unavailable.
    absl::optional<std::u16string> fallback_app_name;

    bool locally_installed = true;
    // These OS shortcut fields can't be true if |locally_installed| is false.
    bool add_to_applications_menu = true;
    bool add_to_desktop = true;
    bool add_to_quick_launch_bar = true;
    bool run_on_os_login = false;

    // These have no effect outside of Chrome OS.
    bool add_to_search = true;
    bool add_to_management = true;
    bool is_disabled = false;

    bool bypass_service_worker_check = false;
    bool require_manifest = false;

    std::vector<std::string> additional_search_terms;

    absl::optional<std::string> launch_query_params;
    absl::optional<SystemAppType> system_app_type;

    bool oem_installed = false;
  };
  // Starts a background web app installation process for a given
  // |web_contents|.
  void InstallWebAppWithParams(content::WebContents* web_contents,
                               const InstallParams& install_params,
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
      const absl::optional<InstallParams>& install_params,
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

 protected:
  Profile* profile() { return profile_; }
  WebAppRegistrar* registrar() { return registrar_; }
  OsIntegrationManager* os_integration_manager() {
    return os_integration_manager_;
  }
  InstallFinalizer* finalizer() { return finalizer_; }

  bool disable_web_app_sync_install_for_testing() const {
    return disable_web_app_sync_install_for_testing_;
  }

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

    CheckedPtr<const WebAppInstallTask> task = nullptr;
    base::OnceClosure start;
  };

  void OnWebContentsReadyRunTask(PendingTask pending_task,
                                 WebAppUrlLoader::Result result);

  DataRetrieverFactory data_retriever_factory_;

  const CheckedPtr<Profile> profile_;
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  CheckedPtr<WebAppRegistrar> registrar_ = nullptr;
  CheckedPtr<OsIntegrationManager> os_integration_manager_ = nullptr;
  CheckedPtr<InstallFinalizer> finalizer_ = nullptr;

  bool disable_web_app_sync_install_for_testing_ = false;

  // All owned tasks.
  using Tasks = base::flat_set<std::unique_ptr<WebAppInstallTask>,
                               base::UniquePtrComparator>;
  Tasks tasks_;

  using TaskQueue = base::queue<PendingTask>;
  TaskQueue task_queue_;
  CheckedPtr<const WebAppInstallTask> current_queued_task_ = nullptr;

  // A single WebContents, shared between tasks in |task_queue_|.
  std::unique_ptr<content::WebContents> web_contents_;

  bool started_ = false;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
