// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_app_registry_controller.h"

#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

TestAppRegistryController::TestAppRegistryController(Profile* profile)
    : TestAppRegistryController(
          profile,
          std::make_unique<TestWebAppDatabaseFactory>(),
          std::make_unique<WebAppRegistrarMutable>(profile)) {}

TestAppRegistryController::TestAppRegistryController(
    Profile* profile,
    std::unique_ptr<TestWebAppDatabaseFactory> database_factory,
    std::unique_ptr<WebAppRegistrarMutable> registrar)
    : WebAppSyncBridge(profile,
                       database_factory.get(),
                       registrar.get(),
                       nullptr),
      database_factory_(std::move(database_factory)),
      registrar_(std::move(registrar)) {}

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

void TestAppRegistryController::SetAppWindowControlsOverlayEnabled(
    const AppId& app_id,
    bool enabled) {}

}  // namespace web_app
