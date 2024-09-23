// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"

class GURL;
class Profile;

namespace webapps {
enum class UninstallResultCode;
}  // namespace webapps

namespace web_app {

// This command acquires the AllAppsLock needed by three uninstall related jobs:
// `RemoveInstallUrlJob`, `RemoveInstallSourceJob`, and `RemoveWebAppJob`.
class WebAppUninstallCommand
    : public WebAppCommand<AllAppsLock, webapps::UninstallResultCode> {
 public:
  static std::unique_ptr<WebAppUninstallCommand> CreateForRemoveInstallUrl(
      webapps::WebappUninstallSource uninstall_source,
      Profile& profile,
      std::optional<webapps::AppId> app_id,
      WebAppManagement::Type install_source,
      GURL install_url,
      UninstallJob::Callback callback);

  static std::unique_ptr<WebAppUninstallCommand>
  CreateForRemoveInstallManagements(
      webapps::WebappUninstallSource uninstall_source,
      Profile& profile,
      webapps::AppId app_id,
      WebAppManagementTypes install_sources,
      UninstallJob::Callback callback);

  static std::unique_ptr<WebAppUninstallCommand>
  CreateForRemoveUserUninstallableManagement(
      webapps::WebappUninstallSource uninstall_source,
      Profile& profile,
      webapps::AppId app_id,
      UninstallJob::Callback callback);

  ~WebAppUninstallCommand() override;

  // WebAppCommand:
  void OnShutdown(base::PassKey<WebAppCommandManager>) const override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  // Constructor for RemoveInstallUrlJob.
  WebAppUninstallCommand(webapps::WebappUninstallSource uninstall_source,
                         Profile& profile,
                         std::optional<webapps::AppId> app_id,
                         WebAppManagement::Type install_source,
                         GURL install_url,
                         UninstallJob::Callback callback);
  // Constructor for RemoveInstallSourceJob.
  WebAppUninstallCommand(webapps::WebappUninstallSource uninstall_source,
                         Profile& profile,
                         webapps::AppId app_id,
                         WebAppManagementTypes install_managements,
                         UninstallJob::Callback callback);
  // Constructor for RemoveInstallSourceJob with user uninstallable sources.
  WebAppUninstallCommand(webapps::WebappUninstallSource uninstall_source,
                         Profile& profile,
                         webapps::AppId app_id,
                         UninstallJob::Callback callback);

  void OnCompletion(webapps::UninstallResultCode code);

  std::unique_ptr<AllAppsLock> lock_;
  std::unique_ptr<UninstallJob> job_;

  base::WeakPtrFactory<WebAppUninstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_
