// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_OS_INTEGRATION_SYNCHRONIZE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_OS_INTEGRATION_SYNCHRONIZE_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

struct SynchronizeOsOptions;

// Used to call OsIntegrationManager::Synchronize() with an app_lock. This
// command can be called without the app being installed in the WebAppProvider
// to use the `SynchronizeOsOptions::force_unregister_os_integration`
// functionality which attempts to remove OS integration for a given app_id even
// if it's not in the database.
class OsIntegrationSynchronizeCommand : public WebAppCommand<AppLock> {
 public:
  OsIntegrationSynchronizeCommand(
      const webapps::AppId& app_id,
      std::optional<SynchronizeOsOptions> synchronize_options,
      bool upgrade_to_fully_installed_if_installed,
      base::OnceClosure synchronize_callback);
  ~OsIntegrationSynchronizeCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> app_lock) override;

 private:
  void OnSynchronizeComplete();

  std::unique_ptr<AppLock> app_lock_;

  webapps::AppId app_id_;
  std::optional<SynchronizeOsOptions> synchronize_options_ = std::nullopt;
  bool upgrade_to_fully_installed_if_installed_;

  base::WeakPtrFactory<OsIntegrationSynchronizeCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_OS_INTEGRATION_SYNCHRONIZE_COMMAND_H_
