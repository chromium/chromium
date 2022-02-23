// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_

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

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_
