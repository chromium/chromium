// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_OS_INTEGRATION_SYNCHRONIZE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_OS_INTEGRATION_SYNCHRONIZE_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class LockDescription;
class AppLock;
class AppLockDescription;
struct SynchronizeOsOptions;

// Used to call OsIntegrationManager::Synchronize() with an app_lock.
class OsIntegrationSynchronizeCommand : public WebAppCommandTemplate<AppLock> {
 public:
  OsIntegrationSynchronizeCommand(
      const webapps::AppId& app_id,
      absl::optional<SynchronizeOsOptions> synchronize_options,
      base::OnceClosure synchronize_callback);
  ~OsIntegrationSynchronizeCommand() override;

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<AppLock> app_lock) override;
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  void OnSynchronizeComplete();

  std::unique_ptr<AppLockDescription> app_lock_description_;
  std::unique_ptr<AppLock> app_lock_;

  webapps::AppId app_id_;
  absl::optional<SynchronizeOsOptions> synchronize_options_ = absl::nullopt;
  base::OnceClosure synchronize_callback_;

  base::WeakPtrFactory<OsIntegrationSynchronizeCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_OS_INTEGRATION_SYNCHRONIZE_COMMAND_H_
