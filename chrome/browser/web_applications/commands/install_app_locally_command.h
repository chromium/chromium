// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_LOCALLY_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_LOCALLY_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class LockDescription;
class AppLock;
class AppLockDescription;

class InstallAppLocallyCommand : public WebAppCommandTemplate<AppLock> {
 public:
  InstallAppLocallyCommand(const AppId& app_id,
                           base::OnceClosure install_callback);
  ~InstallAppLocallyCommand() override;

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<AppLock> app_lock) override;
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  // Records locally installed app success metric to UMA after OS Hooks are
  // installed.
  void OnOsHooksInstalled(const OsHooksErrors os_hooks_errors);
  void ReportResultAndShutdown(CommandResult result);

  std::unique_ptr<AppLockDescription> app_lock_description_;
  std::unique_ptr<AppLock> app_lock_;

  AppId app_id_;
  base::OnceClosure install_callback_;
  base::Value::Dict debug_log_;

  base::WeakPtrFactory<InstallAppLocallyCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_LOCALLY_COMMAND_H_
