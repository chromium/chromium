// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;
class PrefService;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class AllAppsLock;
class AllAppsLockDescription;
class LockDescription;
class WebAppUninstallJob;

// This command is used to uninstall a web_app. Once started, this command will:
// 1. Start maintaining a queue of all app_ids that need to be uninstalled.
// 2. For each app_id:
//     a. If an app was triggered by an external install source (like policy,
//     default or system) and was installed by multiple sources, it will
//     remove the source that triggered the uninstallation. Sometimes doing so
//     might change the behavior of an app from being user uninstallable to not
//     being user uninstallable in which case, OS integration for user
//     uninstallation is unregistered.
//     b. If an app uninstallation was triggered by any other source and if the
//     app was a default app, the app_id is stored in the
//     UserUninstalledPreinstalledWebAppPrefs so that the
//     ExternallyManagedAppManager does not auto synchronize and reinstall the
//     default app on next step. See
//     `ExternallyManagedAppManager::SynchronizeInstalledApps()` for more info.
//     c. If the app being uninstalled is a parent app with multiple sub apps,
//     all sub app IDs are queued onto the overall uninstallation queue.
// 3. For all other use-cases, a WebAppUninstallJob is initialized and kicked
//    off per app_id. The job is owned by the command, and the command keeps
//    track of all currently running jobs.
// 4. The command ends only when both of the conditions below are successful:
//    a. All running WebAppUninstallJobs have been completed.
//    b. The queue that was keeping track of app_ids that needed to be
//    uninstalled is empty.
class WebAppUninstallCommand : public WebAppCommandTemplate<AllAppsLock> {
 public:
  using UninstallWebAppCallback =
      base::OnceCallback<void(webapps::UninstallResultCode)>;
  using RemoveManagementTypeCallback =
      base::RepeatingCallback<void(const AppId& app_id)>;

  WebAppUninstallCommand(
      const AppId& app_id,
      absl::optional<WebAppManagement::Type> management_type_or_all,
      webapps::WebappUninstallSource uninstall_source,
      UninstallWebAppCallback callback,
      Profile* profile);
  ~WebAppUninstallCommand() override;

  // WebAppCommandTemplate<AllAppsLock>:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;

  void SetRemoveManagementTypeCallbackForTesting(
      RemoveManagementTypeCallback callback);

 private:
  // Used to store information needed for uninstalling an app with app_id.
  struct UninstallInfo {
    UninstallInfo(AppId app_id,
                  absl::optional<WebAppManagement::Type> management_type_or_all,
                  webapps::WebappUninstallSource uninstall_source);
    ~UninstallInfo();
    UninstallInfo(const UninstallInfo& uninstall_info);
    UninstallInfo(UninstallInfo&& uninstall_info);
    UninstallInfo& operator=(const UninstallInfo& uninstall_info) = delete;
    UninstallInfo& operator=(UninstallInfo&& uninstall_info) = delete;

    AppId app_id;
    absl::optional<WebAppManagement::Type> management_type_or_all;
    webapps::WebappUninstallSource uninstall_source;
  };

  void AppendUninstallInfoToDebugLog(const UninstallInfo& uninstall_info);
  void AppendUninstallResultsToDebugLog(const AppId& app_id);
  void Abort(webapps::UninstallResultCode code);
  void Uninstall(const AppId& app_id,
                 webapps::WebappUninstallSource uninstall_source);
  void QueueSubAppsForUninstallIfAny(const AppId& app_id);
  void RemoveManagementTypeAfterOsUninstallRegistration(
      const AppId& app_id,
      const WebAppManagement::Type& install_source,
      webapps::WebappUninstallSource uninstall_source,
      OsHooksErrors os_hooks_errors);
  void OnSingleUninstallComplete(const AppId& app_id,
                                 webapps::WebappUninstallSource source,
                                 webapps::UninstallResultCode code);
  void MaybeFinishUninstallAndDestruct();

  std::unique_ptr<AllAppsLockDescription> lock_description_;
  std::unique_ptr<AllAppsLock> lock_;

  const AppId app_id_;
  base::circular_deque<UninstallInfo> queued_uninstalls_;
  base::flat_map<AppId, webapps::UninstallResultCode> uninstall_results_;
  base::flat_map<AppId, std::unique_ptr<WebAppUninstallJob>>
      apps_pending_uninstall_;
  base::Value::Dict debug_log_;
  bool all_uninstalled_queued_ = false;

  UninstallWebAppCallback callback_;
  RemoveManagementTypeCallback management_type_removed_callback_for_testing_;

  raw_ptr<PrefService> profile_prefs_;

  base::WeakPtrFactory<WebAppUninstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_
