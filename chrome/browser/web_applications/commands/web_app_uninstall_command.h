// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/uninstall/uninstall_job.h"

namespace webapps {
enum class UninstallResultCode;
}  // namespace webapps

namespace web_app {

class AllAppsLock;
class AllAppsLockDescription;
class LockDescription;

// This command acquires the AllAppsLock needed by three uninstall related jobs:
// `RemoveInstallUrlJob`, `RemoveInstallSourceJob`, and `RemoveWebAppJob`.
class WebAppUninstallCommand : public WebAppCommandTemplate<AllAppsLock> {
 public:
  WebAppUninstallCommand(std::unique_ptr<UninstallJob> job,
                         UninstallJob::Callback callback);
  ~WebAppUninstallCommand() override;

  // WebAppCommandTemplate<AllAppsLock>:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;
  void OnShutdown() override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;

 private:
  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);

  std::unique_ptr<AllAppsLockDescription> lock_description_;
  std::unique_ptr<AllAppsLock> lock_;

  std::unique_ptr<UninstallJob> job_;
  UninstallJob::Callback callback_;

  base::WeakPtrFactory<WebAppUninstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_
