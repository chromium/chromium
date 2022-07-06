// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class WebAppRegistrar;
class OsIntegrationManager;
class WebAppSyncBridge;

// Sets the callback for the test to wait until the OS integration state is set.
void SetRunOnOsLoginOsHooksChangedCallbackForTesting(
    base::OnceClosure callback);

// Updates the Run On OS state for the given app. If necessary, it also updates
// the registration with the OS. This will take into account the enterprise
// policy for the given app.
void PersistRunOnOsLoginUserChoice(WebAppRegistrar* registrar,
                                   OsIntegrationManager* os_integration_manager,
                                   WebAppSyncBridge* sync_bridge,
                                   const AppId& app_id,
                                   RunOnOsLoginMode new_user_mode);

// Refreshes the Run On OS Login OS integration state for the specified app.
// Note that this tries to avoid extra work by no-oping if the current
// OS state matches what is calculated to be the desired stated.
void SyncRunOnOsLoginOsIntegrationState(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    const AppId& app_id);

enum class RunOnOsLoginAction {
  kPersistMode = 0,
  kSyncModeToSystem = 1,
  kMaxValue = kSyncModeToSystem
};

enum class RunOnOsLoginCommandCompletionState {
  kSuccessfulCompletion = 0,
  kCommandSystemShutDown = 1,
  kNotAllowedByPolicy = 2,
  kRunOnOsLoginModeAlreadyMatched = 3,
  kAppNotLocallyInstalled = 4,
  kOSHooksNotProperlySet = 5,
  kMaxValue = kOSHooksNotProperlySet
};

// This command persists run on os login data to the web_app DB
// and/or syncs the run on os login data with the OS integration hooks
// asynchronously.
class RunOnOsLoginCommand : public WebAppCommand {
 public:
  static std::unique_ptr<RunOnOsLoginCommand> CreateForPersistMode(
      WebAppRegistrar* registrar,
      OsIntegrationManager* os_integration_manager,
      WebAppSyncBridge* sync_bridge,
      const AppId& app_id,
      RunOnOsLoginMode login_mode,
      base::OnceClosure callback);
  static std::unique_ptr<RunOnOsLoginCommand> CreateForSyncMode(
      WebAppRegistrar* registrar,
      OsIntegrationManager* os_integration_manager,
      const AppId& app_id,
      base::OnceClosure callback);
  ~RunOnOsLoginCommand() override;
  void Start() override;
  void OnSyncSourceRemoved() override{};
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  RunOnOsLoginCommand(AppId app_id,
                      WebAppRegistrar* registrar,
                      OsIntegrationManager* os_integration_manager,
                      WebAppSyncBridge* sync_bridge,
                      absl::optional<RunOnOsLoginMode> login_mode,
                      RunOnOsLoginAction persist_or_sync_mode,
                      base::OnceClosure callback_);
  void Abort(RunOnOsLoginCommandCompletionState aborted_state);
  // Updates the Run On OS login state for the given app. If necessary, it also
  // updates the registration with the OS. This will take into account the
  // enterprise policy for the given app.
  void PersistRunOnOsLoginMode();
  // Reads the expected Run On OS login state for a given app from the DB and
  // deploys that to the OS.
  // Note that this tries to avoid extra work by no-oping if the current
  // OS state matches what is calculated to be the desired stated.
  void SyncRunOnOsLoginMode();
  void UpdateRunOnOsLoginModeWithOsIntegration();
  void OnOsHooksSet(OsHooksErrors errors);
  void RecordCompletionState(
      RunOnOsLoginCommandCompletionState completion_state);

  AppId app_id_;
  base::raw_ptr<WebAppRegistrar> registrar_;
  base::raw_ptr<OsIntegrationManager> os_integration_manager_;
  base::raw_ptr<WebAppSyncBridge> sync_bridge_;
  absl::optional<RunOnOsLoginMode> login_mode_;
  RunOnOsLoginAction persist_or_sync_mode_;
  std::string stop_reason_;
  base::OnceClosure callback_ = base::DoNothing();

  base::WeakPtrFactory<RunOnOsLoginCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_
