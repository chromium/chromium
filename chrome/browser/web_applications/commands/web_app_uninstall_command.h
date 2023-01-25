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

class FullSystemLock;
class FullSystemLockDescription;
class LockDescription;
class WebAppUninstallJob;

// Uninstall the web app.
class WebAppUninstallCommand : public WebAppCommandTemplate<FullSystemLock> {
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

  // WebAppCommandTemplate<FullSystemLock>:
  void StartWithLock(std::unique_ptr<FullSystemLock> lock) override;
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
                 const webapps::WebappUninstallSource& uninstall_source);
  void QueueSubAppsForUninstallIfAny(const AppId& app_id);
  void RemoveManagementTypeAfterOsUninstallRegistration(
      const AppId& app_id,
      const WebAppManagement::Type& install_source,
      const webapps::WebappUninstallSource& uninstall_source,
      OsHooksErrors os_hooks_errors);
  void OnSingleUninstallComplete(const AppId& app_id,
                                 const webapps::WebappUninstallSource& source,
                                 webapps::UninstallResultCode code);
  void MaybeFinishUninstallAndDestruct();

  std::unique_ptr<FullSystemLockDescription> lock_description_;
  std::unique_ptr<FullSystemLock> lock_;

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
