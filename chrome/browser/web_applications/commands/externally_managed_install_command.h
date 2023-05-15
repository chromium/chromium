// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNALLY_MANAGED_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNALLY_MANAGED_INSTALL_COMMAND_H_

#include <memory>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_uninstall_and_replace_job.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace web_app {

class LockDescription;
class AppLock;
class AppLockDescription;
class NoopLock;
class NoopLockDescription;
class WebAppDataRetriever;

// Command to install web_apps from param by the ExternallyInstalledAppsManager.
// After loading the install-URL and retrieving all necessary information from
// that page, the WebContent will be navigated to about:blank. Otherwise,
// subsequent icon fetching might get interrupted by navigations that got
// started via HTML or javascript on the install-URL page.
class ExternallyManagedInstallCommand : public WebAppCommandTemplate<NoopLock> {
 public:
  using InstallAndReplaceCallback =
      base::OnceCallback<void(const AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace)>;

  // web_app_url_loader can be nullptr. In that case there will be no navigation
  // to about::blank before retrieving the icons. This might cause the icon
  // fetching to be interrupted by navigations on the web page.
  ExternallyManagedInstallCommand(
      Profile* profile,
      const ExternalInstallOptions& external_install_options,
      InstallAndReplaceCallback callback,
      base::WeakPtr<content::WebContents> contents,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      WebAppUrlLoader* web_app_url_loader);
  ~ExternallyManagedInstallCommand() override;

  // WebAppCommandTemplate<NoopLock>:
  const LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<NoopLock> lock) override;
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

  void SetOnLockUpgradedCallbackForTesting(base::OnceClosure callback);

 private:
  void Abort(webapps::InstallResultCode code);

  void OnGetWebAppInstallInfoInCommand(
      std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    const GURL& manifest_url,
                                    bool valid_manifest_for_web_app,
                                    webapps::InstallableStatusCode error_code);
  void OnPreparedForIconRetrieving(base::flat_set<GURL> icon_urls,
                                   const bool& skip_page_favicons,
                                   WebAppUrlLoaderResult result);
  void OnIconsRetrievedUpgradeLockDescription(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);

  void OnLockUpgradedFinalizeInstall(std::unique_ptr<AppLock> app_lock);

  void OnInstallFinalized(const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);
  void OnUninstallAndReplaced(const AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace);

  const raw_ptr<Profile> profile_;

  std::unique_ptr<NoopLockDescription> noop_lock_description_;
  std::unique_ptr<AppLockDescription> app_lock_description_;

  std::unique_ptr<AppLock> app_lock_;
  std::unique_ptr<NoopLock> noop_lock_;

  WebAppInstallParams install_params_;
  webapps::WebappInstallSource install_surface_;
  const std::vector<AppId> apps_to_uninstall_;
  InstallAndReplaceCallback install_callback_;

  base::WeakPtr<content::WebContents> web_contents_;

  bool bypass_service_worker_check_ = false;
  bool icon_download_failed_ = false;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  const raw_ptr<WebAppUrlLoader, DanglingUntriaged> web_app_url_loader_;

  absl::optional<WebAppUninstallAndReplaceJob> uninstall_and_replace_job_;

  base::Value::Dict debug_value_;
  InstallErrorLogEntry install_error_log_entry_;

  AppId app_id_;

  base::OnceClosure on_lock_upgraded_callback_for_testing_;

  base::WeakPtrFactory<ExternallyManagedInstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNALLY_MANAGED_INSTALL_COMMAND_H_
