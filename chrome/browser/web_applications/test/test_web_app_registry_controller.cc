// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

TestWebAppRegistryController::TestWebAppRegistryController() = default;

TestWebAppRegistryController::~TestWebAppRegistryController() = default;

void TestWebAppRegistryController::SetUp(Profile* profile) {
  database_factory_ = std::make_unique<TestWebAppDatabaseFactory>();
  mutable_registrar_ = std::make_unique<WebAppRegistrarMutable>(profile);

  os_integration_manager_ = std::make_unique<TestOsIntegrationManager>(
      profile, /*app_shortcut_manager=*/nullptr,
      /*file_handler_manager=*/nullptr,
      /*protocol_handler_manager=*/nullptr,
      /*url_handler_manager=*/nullptr);

  mutable_registrar_->SetSubsystems(os_integration_manager_.get());

  sync_bridge_ = std::make_unique<WebAppSyncBridge>(
      profile, database_factory_.get(), mutable_registrar_.get(), this,
      mock_processor_.CreateForwardingProcessor());
  sync_bridge_->SetSubsystems(os_integration_manager_.get());

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

void TestWebAppRegistryController::ApplySyncChanges_AddApps(
    std::vector<GURL> apps_to_add) {
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      sync_bridge().CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;

  for (const GURL& app_url : apps_to_add) {
    const AppId app_id = GenerateAppIdFromURL(app_url);

    auto web_app_server_data = std::make_unique<WebApp>(app_id);
    web_app_server_data->SetName("WebApp name");
    web_app_server_data->SetStartUrl(app_url);
    web_app_server_data->SetUserDisplayMode(DisplayMode::kStandalone);

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "WebApp sync data name";
    sync_fallback_data.theme_color = SK_ColorWHITE;
    web_app_server_data->SetSyncFallbackData(std::move(sync_fallback_data));

    std::unique_ptr<syncer::EntityData> entity_data =
        CreateSyncEntityData(*web_app_server_data);

    auto entity_change =
        syncer::EntityChange::CreateAdd(app_id, std::move(*entity_data));
    entity_changes.push_back(std::move(entity_change));
  }

  sync_bridge().ApplySyncChanges(std::move(metadata_change_list),
                                 std::move(entity_changes));
}

void TestWebAppRegistryController::ApplySyncChanges_UpdateApps(
    const std::vector<std::unique_ptr<WebApp>>& apps_server_state) {
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      sync_bridge().CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;

  for (const std::unique_ptr<WebApp>& web_app_server_state :
       apps_server_state) {
    std::unique_ptr<syncer::EntityData> entity_data =
        CreateSyncEntityData(*web_app_server_state);

    auto entity_change = syncer::EntityChange::CreateUpdate(
        web_app_server_state->app_id(), std::move(*entity_data));
    entity_changes.push_back(std::move(entity_change));
  }

  sync_bridge().ApplySyncChanges(std::move(metadata_change_list),
                                 std::move(entity_changes));
}

void TestWebAppRegistryController::ApplySyncChanges_DeleteApps(
    std::vector<AppId> app_ids_to_delete) {
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      sync_bridge().CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;

  for (const AppId& app_id : app_ids_to_delete) {
    auto entity_change = syncer::EntityChange::CreateDelete(app_id);
    entity_changes.push_back(std::move(entity_change));
  }

  sync_bridge().ApplySyncChanges(std::move(metadata_change_list),
                                 std::move(entity_changes));
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
  os_integration_manager_.reset();
}

}  // namespace web_app
