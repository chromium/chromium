// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom-shared.h"
#include "url/gurl.h"

struct WebAppInstallInfo;

class Profile;

namespace web_app {

class LockDescription;
class SharedWebContentsWithAppLock;
class SharedWebContentsWithAppLockDescription;
class WebAppUrlLoader;
class WebAppDataRetriever;

using AppInstallResults =
    std::vector<std::pair<AppId, blink::mojom::SubAppsServiceResult>>;
using SubAppInstallResultCallback = base::OnceCallback<void(AppInstallResults)>;

class SubAppInstallCommand
    : public WebAppCommandTemplate<SharedWebContentsWithAppLock> {
 public:
  SubAppInstallCommand(const AppId& parent_app_id,
                       std::vector<std::pair<UnhashedAppId, GURL>> sub_apps,
                       SubAppInstallResultCallback install_callback,
                       Profile* profile,
                       std::unique_ptr<WebAppUrlLoader> url_loader,
                       std::unique_ptr<WebAppDataRetriever> data_retriever);
  ~SubAppInstallCommand() override;
  SubAppInstallCommand(const SubAppInstallCommand&) = delete;
  SubAppInstallCommand& operator=(const SubAppInstallCommand&) = delete;

  // WebAppCommandTemplate<SharedWebContentsWithAppLock>:
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void SetDialogNotAcceptedForTesting();

 protected:
  // WebAppCommandTemplate<SharedWebContentsWithAppLock>:
  void StartWithLock(
      std::unique_ptr<SharedWebContentsWithAppLock> lock) override;
  void OnSyncSourceRemoved() override {}
  void OnShutdown() override;

 private:
  enum class State {
    kNotStarted = 0,
    kPendingDialogCallbacks = 1,
    kCallbacksComplete = 2
  } state_ = State::kNotStarted;

  // Functions to perform install flow for each sub app.
  void StartNextInstall();
  void OnWebAppUrlLoadedGetWebAppInstallInfo(
      const UnhashedAppId& unhashed_app_id,
      const GURL& url_to_load,
      WebAppUrlLoader::Result result);
  void OnGetWebAppInstallInfo(const UnhashedAppId& unhashed_app_id,
                              std::unique_ptr<WebAppInstallInfo> install_info);
  void OnDidPerformInstallableCheck(
      const UnhashedAppId& unhashed_app_id,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);
  void OnIconsRetrievedShowDialog(
      const UnhashedAppId& unhashed_app_id,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnDialogCompleted(const UnhashedAppId& unhashed_app_id,
                         bool user_accepted,
                         std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnInstallFinalized(const UnhashedAppId& unhashed_app_id,
                          const GURL& start_url,
                          const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);
  void MaybeFinishInstall(const UnhashedAppId& app_id,
                          webapps::InstallResultCode code);

  // Functions to manage all sub apps installations.
  void MaybeShowDialog();
  void MaybeFinishCommand();
  void AddResultAndRemoveFromPendingInstalls(
      const UnhashedAppId& unhashed_app_id,
      webapps::InstallResultCode result);
  bool IsWebContentsDestroyed();
  void AddResultToDebugData(const UnhashedAppId& unhashed_app_id,
                            const GURL& url,
                            const AppId& installed_app_id,
                            webapps::InstallResultCode detailed_code,
                            const blink::mojom::SubAppsServiceResult& code);

  std::unique_ptr<SharedWebContentsWithAppLockDescription> lock_description_;
  std::unique_ptr<SharedWebContentsWithAppLock> lock_;

  const AppId parent_app_id_;
  std::vector<std::pair<UnhashedAppId, GURL>> requested_installs_;
  SubAppInstallResultCallback install_callback_;

  raw_ptr<Profile> profile_;
  std::unique_ptr<WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::flat_map<UnhashedAppId, GURL> pending_installs_map_;
  size_t num_pending_dialog_callbacks_ = 0;
  AppInstallResults results_;
  InstallErrorLogEntry log_entry_;
  base::Value::Dict debug_install_results_;
  std::vector<
      std::tuple<UnhashedAppId,
                 std::unique_ptr<WebAppInstallInfo>,
                 base::OnceCallback<void(bool user_accepted,
                                         std::unique_ptr<WebAppInstallInfo>)>>>
      acceptance_callbacks_;

  // Testing variables.
  bool dialog_not_accepted_for_testing_ = false;

  base::WeakPtrFactory<SubAppInstallCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_
