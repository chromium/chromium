// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

TestWebAppRegistryController::TestWebAppRegistryController() = default;

TestWebAppRegistryController::~TestWebAppRegistryController() = default;

void TestWebAppRegistryController::SetUp(Profile* profile) {
  database_factory_ = std::make_unique<TestWebAppDatabaseFactory>();
  mutable_registrar_ = std::make_unique<WebAppRegistrarMutable>(profile);

  sync_bridge_ = std::make_unique<WebAppSyncBridge>(
      profile, database_factory_.get(), mutable_registrar_.get(), this,
      mock_processor_.CreateForwardingProcessor());

  ON_CALL(processor(), IsTrackingMetadata())
      .WillByDefault(testing::Return(true));
}

void TestWebAppRegistryController::Init() {
  base::RunLoop run_loop;
  sync_bridge_->Init(run_loop.QuitClosure());
  run_loop.Run();
}

void TestWebAppRegistryController::RegisterApp(
    std::unique_ptr<WebApp> web_app) {
  ScopedRegistryUpdate update(sync_bridge_.get());
  update->CreateApp(std::move(web_app));
}

void TestWebAppRegistryController::UnregisterApp(const AppId& app_id) {
  ScopedRegistryUpdate update(sync_bridge_.get());
  update->DeleteApp(app_id);
}

void TestWebAppRegistryController::UnregisterAll() {
  ScopedRegistryUpdate update(sync_bridge_.get());
  for (const AppId& app_id : registrar().GetAppIds())
    update->DeleteApp(app_id);
}

void TestWebAppRegistryController::SetInstallWebAppsAfterSyncDelegate(
    InstallWebAppsAfterSyncDelegate delegate) {
  install_web_apps_after_sync_delegate_ = delegate;
}

void TestWebAppRegistryController::SetUninstallWebAppsAfterSyncDelegate(
    UninstallWebAppsAfterSyncDelegate delegate) {
  uninstall_web_apps_after_sync_delegate_ = delegate;
}

void TestWebAppRegistryController::InstallWebAppsAfterSync(
    std::vector<WebApp*> web_apps,
    RepeatingInstallCallback callback) {
  if (install_web_apps_after_sync_delegate_) {
    install_web_apps_after_sync_delegate_.Run(std::move(web_apps), callback);
  } else {
    for (WebApp* web_app : web_apps)
      callback.Run(web_app->app_id(), InstallResultCode::kSuccessNewInstall);
  }
}

void TestWebAppRegistryController::UninstallWebAppsAfterSync(
    std::vector<std::unique_ptr<WebApp>> web_apps,
    RepeatingUninstallCallback callback) {
  if (uninstall_web_apps_after_sync_delegate_) {
    uninstall_web_apps_after_sync_delegate_.Run(std::move(web_apps), callback);
  } else {
    for (const std::unique_ptr<WebApp>& web_app : web_apps)
      callback.Run(web_app->app_id(), /*uninstalled=*/true);
  }
}

void TestWebAppRegistryController::DestroySubsystems() {
  mutable_registrar_.reset();
  sync_bridge_.reset();
  database_factory_.reset();
}

}  // namespace web_app
