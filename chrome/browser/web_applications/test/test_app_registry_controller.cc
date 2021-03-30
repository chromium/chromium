// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_app_registry_controller.h"

namespace web_app {

TestAppRegistryController::TestAppRegistryController(Profile* profile)
    : AppRegistryController(profile) {}
TestAppRegistryController::~TestAppRegistryController() = default;

void TestAppRegistryController::Init(base::OnceClosure callback) {
  std::move(callback).Run();
}

void TestAppRegistryController::SetAppUserDisplayMode(const AppId& app_id,
                                                      DisplayMode display_mode,
                                                      bool is_user_action) {}

void TestAppRegistryController::SetAppIsDisabled(const AppId& app_id,
                                                 bool is_disabled) {}
void TestAppRegistryController::UpdateAppsDisableMode() {}

void TestAppRegistryController::SetAppIsLocallyInstalled(
    const AppId& app_id,
    bool is_locally_installed) {}

void TestAppRegistryController::SetAppLastBadgingTime(const AppId& app_id,
                                                      const base::Time& time) {}

void TestAppRegistryController::SetAppLastLaunchTime(const AppId& app_id,
                                                     const base::Time& time) {}

void TestAppRegistryController::SetAppInstallTime(const AppId& app_id,
                                                  const base::Time& time) {}

void TestAppRegistryController::SetAppRunOnOsLoginMode(const AppId& app_id,
                                                       RunOnOsLoginMode mode) {}

WebAppSyncBridge* TestAppRegistryController::AsWebAppSyncBridge() {
  return nullptr;
}

}  // namespace web_app
