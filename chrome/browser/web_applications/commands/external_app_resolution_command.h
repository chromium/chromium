// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNAL_APP_RESOLUTION_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNAL_APP_RESOLUTION_COMMAND_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/jobs/install_from_info_job.h"
#include "chrome/browser/web_applications/jobs/install_placeholder_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class InstallPlaceholderJob;
class WebAppDataRetriever;
class WebAppUninstallAndReplaceJob;

struct WebAppInstallInfo;

// Invariant: This command assumes that a new placeholder app for this
// install_url cannot be installed between the scheduling and running of this
// command.
class ExternalAppResolutionCommand
    : public WebAppCommandTemplate<SharedWebContentsLock> {
 public:
  using InstalledCallback =
      base::OnceCallback<void(ExternallyManagedAppManager::InstallResult)>;

  ExternalAppResolutionCommand(Profile& profile,
                               const ExternalInstallOptions& install_options,
                               absl::optional<AppId> placeholder_app_id,
                               InstalledCallback installed_callback);
  ~ExternalAppResolutionCommand() override;

  // WebAppCommandTemplate<SharedWebContentsLock>:
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void OnShutdown() override;
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);
  void SetOnLockUpgradedCallbackForTesting(base::OnceClosure callback);

 private:
  void Abort(webapps::InstallResultCode code);

  // This function loads the URL and based on the result branches off to try
  // either
  // * regular web app installation (if loading was successful).
  // * placeholder installation (in case loading was successful but the user
  // agent got redirected to a different origin and a placeholder installation
  // is configured as fallback).
  // * offline installation from install info (in all other cases).
  void OnUrlLoadedAndBranchInstallation(WebAppUrlLoader::Result result);

  // Regular installation path:
  void OnGetWebAppInstallInfoInCommand(
      std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    const GURL& manifest_url,
                                    bool valid_manifest_for_web_app,
                                    webapps::InstallableStatusCode error_code);
  void OnPreparedForIconRetrieving(base::flat_set<GURL> icon_urls,
                                   bool skip_page_favicons,
                                   WebAppUrlLoaderResult result);
  void OnIconsRetrievedUpgradeLockDescription(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnLockUpgradedFinalizeInstall(
      bool icon_download_failed,
      std::unique_ptr<SharedWebContentsWithAppLock> apps_lock);
  void OnInstallFinalized(const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);
  void OnUninstallAndReplaceCompletedUninstallPlaceholder(
      AppId app_id,
      webapps::InstallResultCode code,
      bool uninstall_triggered);

  // Placeholder installation path:
  void OnPlaceHolderAppLockAcquired(
      std::unique_ptr<SharedWebContentsWithAppLock> apps_lock);
  void OnPlaceHolderInstalled(webapps::InstallResultCode code, AppId app_id);

  // Offline installation path:
  void InstallFromInfo();
  void OnInstallFromInfoAppLockAcquired(
      std::unique_ptr<SharedWebContentsWithAppLock> apps_lock);
  void OnInstallFromInfoCompleted(const AppId& app_id,
                                  webapps::InstallResultCode code,
                                  OsHooksErrors os_hook_errors);
  void OnUninstallAndReplaceCompleted(bool is_offline_install,
                                      AppId app_id,
                                      webapps::InstallResultCode code,
                                      bool uninstall_triggered);

  void TryAppInfoFactoryOnFailure(
      ExternallyManagedAppManager::InstallResult result);

  ExternallyManagedAppManager::InstallResult PrepareResult(
      bool is_offline_install,
      ExternallyManagedAppManager::InstallResult result);
  void OnInstallationJobsCompleted(bool success,
                                   base::OnceClosure result_closure);

  // SharedWebContentsLock is held while loading the app contents (and manifest,
  // if possible).
  std::unique_ptr<SharedWebContentsLockDescription>
      web_contents_lock_description_;
  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;

  // SharedWebContentsWithAppLock is held when the affected app ids are known
  // (i.e. after deciding whether a placeholder install is needed or when the
  // manifest is loaded).
  std::unique_ptr<SharedWebContentsWithAppLock> apps_lock_;
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
      apps_lock_description_;

  absl::optional<AppId> app_id_;

  // `this` must be owned by `profile_`.
  raw_ref<Profile> profile_;

  InstalledCallback installed_callback_;

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  ExternalInstallOptions install_options_;

  absl::optional<AppId> installed_placeholder_app_id_;

  bool bypass_service_worker_check_ = false;
  webapps::WebappInstallSource install_surface_;
  absl::optional<WebAppInstallParams> install_params_;

  absl::optional<WebAppUninstallAndReplaceJob> uninstall_and_replace_job_;
  absl::optional<InstallPlaceholderJob> install_placeholder_job_;
  absl::optional<InstallFromInfoJob> install_from_info_job_;

  base::Value::List error_log_;
  base::Value::Dict debug_value_;
  InstallErrorLogEntry install_error_log_entry_;

  base::OnceClosure on_lock_upgraded_callback_for_testing_;

  base::WeakPtrFactory<ExternalAppResolutionCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNAL_APP_RESOLUTION_COMMAND_H_
