// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UNINSTALL_ALL_USER_INSTALLED_WEB_APPS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UNINSTALL_ALL_USER_INSTALLED_WEB_APPS_COMMAND_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace webapps {
enum class UninstallResultCode;
}  // namespace webapps

namespace web_app {

// This command acquires the AllAppsLock and uninstalls all user-installed web
// apps.
class UninstallAllUserInstalledWebAppsCommand
    : public WebAppCommand<AllAppsLock, const std::optional<std::string>&> {
 public:
  using Callback =
      base::OnceCallback<void(const std::optional<std::string>& error_message)>;

  UninstallAllUserInstalledWebAppsCommand(
      webapps::WebappUninstallSource uninstall_source,
      Profile& profile,
      Callback callback);
  ~UninstallAllUserInstalledWebAppsCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  void ProcessNextUninstallOrComplete();
  void JobComplete(WebAppManagementTypes types,
                   webapps::UninstallResultCode code);

  std::unique_ptr<AllAppsLock> lock_;

  webapps::WebappUninstallSource uninstall_source_;
  const raw_ref<Profile> profile_;

  std::vector<std::string> errors_;
  std::vector<webapps::AppId> ids_to_uninstall_;
  std::unique_ptr<RemoveInstallSourceJob> active_job_;

  base::WeakPtrFactory<UninstallAllUserInstalledWebAppsCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UNINSTALL_ALL_USER_INSTALLED_WEB_APPS_COMMAND_H_
