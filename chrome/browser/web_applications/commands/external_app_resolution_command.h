// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNAL_APP_RESOLUTION_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNAL_APP_RESOLUTION_COMMAND_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/jobs/install_from_info_job.h"
#include "chrome/browser/web_applications/jobs/install_placeholder_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace webapps {
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

class InstallPlaceholderJob;
class WebAppDataRetriever;
class WebAppUninstallAndReplaceJob;

struct WebAppInstallInfo;

// Invariant: This command assumes that a new placeholder app for this
// install_url cannot be installed between the scheduling and running of this
// command.
class ExternalAppResolutionCommand
    : public WebAppCommand<SharedWebContentsLock,
                           ExternallyManagedAppManager::InstallResult> {
 public:
  using InstallResult = ExternallyManagedAppManager::InstallResult;
  using InstalledCallback = base::OnceCallback<void(InstallResult)>;

  ExternalAppResolutionCommand(Profile& profile,
                               const ExternalInstallOptions& install_options,
                               std::optional<webapps::AppId> placeholder_app_id,
                               InstalledCallback installed_callback);
  ~ExternalAppResolutionCommand() override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);
  void SetOnLockUpgradedCallbackForTesting(base::OnceClosure callback);

  // WebAppCommand:
  void OnShutdown(base::PassKey<WebAppCommandManager>) const override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  WebAppProvider& provider() const;

  void Abort(webapps::InstallResultCode code);

  // This function loads the URL and based on the result branches off to try
  // either
  // * regular web app installation (if loading was successful).
  // * placeholder installation (in case loading was successful but the user
  // agent got redirected to a different origin and a placeholder installation
  // is configured as fallback).
  // * offline installation from install info (in all other cases).
  void OnUrlLoadedAndBranchInstallation(webapps::WebAppUrlLoaderResult result);

  // Regular installation path:
  void OnGetWebAppInstallInfoInCommand(
      std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    bool valid_manifest_for_web_app,
                                    webapps::InstallableStatusCode error_code);
  void OnPreparedForIconRetrieving(IconUrlSizeSet icon_urls,
                                   bool skip_page_favicons,
                                   webapps::WebAppUrlLoaderResult result);
  void OnIconsRetrievedUpgradeLockDescription(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnLockUpgradedFinalizeInstall(bool icon_download_failed);
  void OnInstallFinalized(const webapps::AppId& app_id,
                          webapps::InstallResultCode code);
  void OnUninstallAndReplaceCompletedUninstallPlaceholder(
      bool uninstall_triggered);
  void OnAllAppsLockGrantedRemovePlaceholder();
  void OnPlaceholderUninstalledMaybeRelaunch(
      webapps::UninstallResultCode result);

  void OnLaunch(base::WeakPtr<Browser> browser,
                base::WeakPtr<content::WebContents> web_contents,
                apps::LaunchContainer container,
                base::Value debug_value);

  // Placeholder installation path:
  void OnPlaceHolderAppLockAcquired();
  void OnPlaceHolderInstalled(webapps::InstallResultCode code,
                              webapps::AppId app_id);

  // Offline installation path:
  void InstallFromInfo();
  void OnInstallFromInfoAppLockAcquired();
  void OnInstallFromInfoCompleted(webapps::AppId app_id,
                                  webapps::InstallResultCode code);
  void OnUninstallAndReplaceCompleted(bool is_offline_install,
                                      bool uninstall_triggered);

  void TryAppInfoFactoryOnFailure(
      ExternallyManagedAppManager::InstallResult result);

  ExternallyManagedAppManager::InstallResult PrepareResult(
      bool is_offline_install,
      ExternallyManagedAppManager::InstallResult result);

  // SharedWebContentsLock is held while loading the app contents (and manifest,
  // if possible).
  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;

  // SharedWebContentsWithAppLock is held when the affected app ids are known
  // (i.e. after deciding whether a placeholder install is needed or when the
  // manifest is loaded).
  std::unique_ptr<SharedWebContentsWithAppLock> apps_lock_;

  std::unique_ptr<AllAppsLock> all_apps_lock_;
  std::unique_ptr<AllAppsLockDescription> all_apps_lock_description_;

  // Populated after an install completes.
  webapps::AppId app_id_;
  webapps::InstallResultCode install_code_;
  bool uninstalled_for_replace_ = false;
  bool relaunch_app_after_placeholder_uninstall_ = false;

  // `this` must be owned by `profile_`.
  raw_ref<Profile> profile_;

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  ExternalInstallOptions install_options_;

  std::optional<webapps::AppId> installed_placeholder_app_id_;

  webapps::WebappInstallSource install_surface_;
  std::optional<WebAppInstallParams> install_params_;

  std::optional<WebAppUninstallAndReplaceJob> uninstall_and_replace_job_;
  std::optional<InstallPlaceholderJob> install_placeholder_job_;
  std::optional<InstallFromInfoJob> install_from_info_job_;
  std::optional<RemoveInstallSourceJob> remove_placeholder_job_;

  InstallErrorLogEntry install_error_log_entry_;

  base::OnceClosure on_lock_upgraded_callback_for_testing_;

  base::WeakPtrFactory<ExternalAppResolutionCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNAL_APP_RESOLUTION_COMMAND_H_
