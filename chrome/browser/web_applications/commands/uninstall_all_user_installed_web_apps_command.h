// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UNINSTALL_ALL_USER_INSTALLED_WEB_APPS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UNINSTALL_ALL_USER_INSTALLED_WEB_APPS_COMMAND_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"

namespace webapps {
enum class UninstallResultCode;
}  // namespace webapps

namespace web_app {

class AllAppsLock;
class AllAppsLockDescription;
class LockDescription;

// This command acquires the AllAppsLock and uninstalls all user-installed web
// apps.
class UninstallAllUserInstalledWebAppsCommand
    : public WebAppCommandTemplate<AllAppsLock> {
 public:
  using Callback = base::OnceCallback<void(
      const absl::optional<std::string>& error_message)>;

  UninstallAllUserInstalledWebAppsCommand(
      webapps::WebappUninstallSource uninstall_source,
      Profile& profile,
      Callback callback);
  ~UninstallAllUserInstalledWebAppsCommand() override;

  // WebAppCommandTemplate<AllAppsLock>:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;
  void OnShutdown() override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;

 private:
  void ProcessNextUninstallOrComplete();
  void JobComplete(WebAppManagement::Type install_source,
                   webapps::UninstallResultCode code);
  void CompleteAndSelfDestruct(CommandResult result);

  std::unique_ptr<AllAppsLockDescription> lock_description_;
  std::unique_ptr<AllAppsLock> lock_;

  webapps::WebappUninstallSource uninstall_source_;
  const raw_ref<Profile> profile_;
  Callback callback_;

  std::vector<webapps::AppId> ids_to_uninstall_;
  std::vector<std::pair<std::unique_ptr<RemoveInstallSourceJob>,
                        WebAppManagement::Type>>
      pending_jobs_;
  std::unique_ptr<RemoveInstallSourceJob> active_job_;

  std::vector<std::string> errors_;
  base::Value::Dict debug_info_;

  base::WeakPtrFactory<UninstallAllUserInstalledWebAppsCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UNINSTALL_ALL_USER_INSTALLED_WEB_APPS_COMMAND_H_
