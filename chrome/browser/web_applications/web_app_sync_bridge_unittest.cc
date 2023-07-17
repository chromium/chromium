// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_sync_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

using testing::_;

using AppsList = std::vector<std::unique_ptr<WebApp>>;

void RemoveWebAppFromAppsList(AppsList* apps_list, const AppId& app_id) {
  base::EraseIf(*apps_list, [app_id](const std::unique_ptr<WebApp>& app) {
    return app->app_id() == app_id;
  });
}

bool IsSyncDataEqualIfApplied(const WebApp& expected_app,
                              std::unique_ptr<WebApp> app_to_apply_sync_data,
                              const syncer::EntityData& entity_data) {
  if (!entity_data.specifics.has_web_app())
    return false;

  const GURL sync_start_url(entity_data.specifics.web_app().start_url());
  if (expected_app.app_id() !=
      GenerateAppId(/*manifest_id=*/absl::nullopt, sync_start_url))
    return false;

  // ApplySyncDataToApp enforces kSync source on |app_to_apply_sync_data|.
  ApplySyncDataToApp(entity_data.specifics.web_app(),
                     app_to_apply_sync_data.get());
  app_to_apply_sync_data->SetName(entity_data.name);
  return expected_app == *app_to_apply_sync_data;
}

bool IsSyncDataEqual(const WebApp& expected_app,
                     const syncer::EntityData& entity_data) {
  auto app_to_apply_sync_data = std::make_unique<WebApp>(expected_app.app_id());
  return IsSyncDataEqualIfApplied(
      expected_app, std::move(app_to_apply_sync_data), entity_data);
}

bool RegistryContainsSyncDataBatchChanges(
    const Registry& registry,
    std::unique_ptr<syncer::DataBatch> data_batch) {
  if (!data_batch || !data_batch->HasNext())
    return registry.empty();

  while (data_batch->HasNext()) {
    syncer::KeyAndData key_and_data = data_batch->Next();
    auto web_app_iter = registry.find(key_and_data.first);
    if (web_app_iter == registry.end()) {
      LOG(ERROR) << "App not found in registry: " << key_and_data.first;
      return false;
    }

    if (!IsSyncDataEqual(*web_app_iter->second, *key_and_data.second))
      return false;
  }
  return true;
}

std::unique_ptr<WebApp> CreateWebAppWithSyncOnlyFields(const std::string& url) {
  const GURL start_url(url);
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(WebAppManagement::kSync);
  web_app->SetStartUrl(start_url);
  web_app->SetName("Name");
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  return web_app;
}

AppsList CreateAppsList(const std::string& base_url, int num_apps) {
  AppsList apps_list;

  for (int i = 0; i < num_apps; ++i) {
    apps_list.push_back(
        CreateWebAppWithSyncOnlyFields(base_url + base::NumberToString(i)));
  }

  return apps_list;
}

void InsertAppIntoRegistry(Registry* registry, std::unique_ptr<WebApp> app) {
  AppId app_id = app->app_id();
  ASSERT_FALSE(base::Contains(*registry, app_id));
  registry->emplace(std::move(app_id), std::move(app));
}

void InsertAppsListIntoRegistry(Registry* registry, const AppsList& apps_list) {
  for (const std::unique_ptr<WebApp>& app : apps_list)
    registry->emplace(app->app_id(), std::make_unique<WebApp>(*app));
}

void ConvertAppToEntityChange(const WebApp& app,
                              syncer::EntityChange::ChangeType change_type,
                              syncer::EntityChangeList* sync_data_list) {
  std::unique_ptr<syncer::EntityChange> entity_change;

  switch (change_type) {
    case syncer::EntityChange::ACTION_ADD:
      entity_change = syncer::EntityChange::CreateAdd(
          app.app_id(), std::move(*CreateSyncEntityData(app)));
      break;
    case syncer::EntityChange::ACTION_UPDATE:
      entity_change = syncer::EntityChange::CreateUpdate(
          app.app_id(), std::move(*CreateSyncEntityData(app)));
      break;
    case syncer::EntityChange::ACTION_DELETE:
      entity_change = syncer::EntityChange::CreateDelete(app.app_id());
      break;
  }

  sync_data_list->push_back(std::move(entity_change));
}

void ConvertAppsListToEntityChangeList(
    const AppsList& apps_list,
    syncer::EntityChangeList* sync_data_list) {
  for (auto& app : apps_list) {
    ConvertAppToEntityChange(*app, syncer::EntityChange::ACTION_ADD,
                             sync_data_list);
  }
}

// Returns true if the app converted from entity_data exists in the apps_list.
bool RemoveEntityDataAppFromAppsList(const std::string& storage_key,
                                     const syncer::EntityData& entity_data,
                                     AppsList* apps_list) {
  for (auto& app : *apps_list) {
    if (app->app_id() == storage_key) {
      if (!IsSyncDataEqual(*app, entity_data))
        return false;

      RemoveWebAppFromAppsList(apps_list, storage_key);
      return true;
    }
  }

  return false;
}

void RunCallbacksOnInstall(
    const std::vector<WebApp*>& apps,
    const WebAppSyncBridge::RepeatingInstallCallback& callback,
    webapps::InstallResultCode code) {
  for (WebApp* app : apps)
    callback.Run(app->app_id(), code);
}

}  // namespace

class WebAppSyncBridgeTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    command_manager_ = std::make_unique<WebAppCommandManager>(profile());
    command_scheduler_ = std::make_unique<WebAppCommandScheduler>(*profile());
    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    registrar_mutable_ = std::make_unique<WebAppRegistrarMutable>(profile());
    sync_bridge_ = std::make_unique<WebAppSyncBridge>(
        registrar_mutable_.get(), mock_processor_.CreateForwardingProcessor());
    database_factory_ = std::make_unique<FakeWebAppDatabaseFactory>();

    sync_bridge_->SetSubsystems(database_factory_.get(), command_manager_.get(),
                                command_scheduler_.get(),
                                install_manager_.get());

    install_manager_->Start();

    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  void InitSyncBridge() {
    base::RunLoop loop;
    sync_bridge_->Init(loop.QuitClosure());
    loop.Run();
  }

  void InitSyncBridgeFromAppList(const AppsList& apps_list) {
    Registry registry;
    InsertAppsListIntoRegistry(&registry, apps_list);
    database_factory().WriteRegistry(registry);
    InitSyncBridge();
  }

  void MergeFullSyncData(const AppsList& merged_apps) {
    syncer::EntityChangeList entity_data_list;
    ConvertAppsListToEntityChangeList(merged_apps, &entity_data_list);
    EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
    EXPECT_CALL(processor(), Delete(_, _)).Times(0);
    sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                    std::move(entity_data_list));
  }

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(registrar_registry(), registry);
  }

  void SetSyncInstallCallbackFailureIfCalled() {
    sync_bridge().SetInstallWebAppsAfterSyncCallbackForTesting(
        base::BindLambdaForTesting(
            [&](std::vector<WebApp*> apps_to_install,
                WebAppSyncBridge::RepeatingInstallCallback callback) {
              ADD_FAILURE();
            }));

    sync_bridge().SetUninstallFromSyncCallbackForTesting(
        base::BindLambdaForTesting(
            [&](const std::vector<AppId>& apps_to_uninstall,
                WebAppSyncBridge::RepeatingUninstallCallback callback) {
              ADD_FAILURE();
            }));
  }

 protected:
  void DestroyManagers() {
    if (command_manager_) {
      command_manager_->Shutdown();
      command_manager_.reset();
    }
    if (command_scheduler_) {
      command_scheduler_->Shutdown();
      command_scheduler_.reset();
    }
    if (install_manager_) {
      install_manager_.reset();
    }
    if (registrar_mutable_) {
      registrar_mutable_.reset();
    }
    if (sync_bridge_) {
      sync_bridge_.reset();
    }
    if (database_factory_) {
      database_factory_.reset();
    }
  }

  syncer::MockModelTypeChangeProcessor& processor() { return mock_processor_; }
  FakeWebAppDatabaseFactory& database_factory() { return *database_factory_; }

  WebAppRegistrar& registrar() { return *registrar_mutable_; }

  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }

  WebAppInstallManager& install_manager() { return *install_manager_; }

  Registry& registrar_registry() { return registrar_mutable_->registry(); }

 private:
  std::unique_ptr<WebAppCommandManager> command_manager_;
  std::unique_ptr<WebAppCommandScheduler> command_scheduler_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppRegistrarMutable> registrar_mutable_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
  std::unique_ptr<FakeWebAppDatabaseFactory> database_factory_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
};

// Tests that the WebAppSyncBridge correctly reports data from the
// WebAppDatabase.
TEST_F(WebAppSyncBridgeTest, GetData) {
  Registry registry;

  std::unique_ptr<WebApp> synced_app1 =
      CreateWebAppWithSyncOnlyFields("https://example.com/app1/");
  {
    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Sync Name";
    sync_fallback_data.theme_color = SK_ColorCYAN;
    synced_app1->SetSyncFallbackData(std::move(sync_fallback_data));
  }
  InsertAppIntoRegistry(&registry, std::move(synced_app1));

  std::unique_ptr<WebApp> synced_app2 =
      CreateWebAppWithSyncOnlyFields("https://example.com/app2/");
  // sync_fallback_data is empty for this app.
  InsertAppIntoRegistry(&registry, std::move(synced_app2));

  std::unique_ptr<WebApp> policy_app = test::CreateWebApp(
      GURL("https://example.org/"), WebAppManagement::kPolicy);
  InsertAppIntoRegistry(&registry, std::move(policy_app));

  database_factory().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
  InitSyncBridge();

  {
    WebAppSyncBridge::StorageKeyList storage_keys;
    // Add an unknown key to test this is handled gracefully.
    storage_keys.push_back("unknown");
    for (const Registry::value_type& id_and_web_app : registry)
      storage_keys.push_back(id_and_web_app.first);

    base::RunLoop run_loop;
    sync_bridge().GetData(
        std::move(storage_keys),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<syncer::DataBatch> data_batch) {
              EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
                  registry, std::move(data_batch)));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    sync_bridge().GetAllDataForDebugging(base::BindLambdaForTesting(
        [&](std::unique_ptr<syncer::DataBatch> data_batch) {
          EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
              registry, std::move(data_batch)));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Tests that the client & storage tags are correct for entity data.
TEST_F(WebAppSyncBridgeTest, Identities) {
  std::unique_ptr<WebApp> app =
      CreateWebAppWithSyncOnlyFields("https://example.com/");
  std::unique_ptr<syncer::EntityData> entity_data = CreateSyncEntityData(*app);

  EXPECT_EQ(app->app_id(), sync_bridge().GetClientTag(*entity_data));
  EXPECT_EQ(app->app_id(), sync_bridge().GetStorageKey(*entity_data));
}

// Test that a empty local data results in no changes sent to the sync system.
TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetAndServerSetAreEmpty) {
  InitSyncBridge();

  syncer::EntityChangeList sync_data_list;

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetEqualsServerSet) {
  AppsList apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  // The incoming list of apps from the sync server.
  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(apps, &sync_data_list);

  // The local app state is the same as the server state, so no changes should
  // be sent.
  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetGreaterThanServerSet) {
  AppsList local_and_server_apps = CreateAppsList("https://example.com/", 10);
  AppsList expected_local_apps_to_upload =
      CreateAppsList("https://example.org/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, local_and_server_apps);
  InsertAppsListIntoRegistry(&registry, expected_local_apps_to_upload);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  auto metadata_change_list = sync_bridge().CreateMetadataChangeList();
  syncer::MetadataChangeList* metadata_ptr = metadata_change_list.get();

  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(local_and_server_apps, &sync_data_list);

  // MergeFullSyncData below should send |expected_local_apps_to_upload| to the
  // processor() to upload to USS.
  base::RunLoop run_loop;
  ON_CALL(processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_EQ(metadata_ptr, metadata);
        EXPECT_TRUE(RemoveEntityDataAppFromAppsList(
            storage_key, *entity_data, &expected_local_apps_to_upload));
        if (expected_local_apps_to_upload.empty())
          run_loop.Quit();
      });

  sync_bridge().MergeFullSyncData(std::move(metadata_change_list),
                                  std::move(sync_data_list));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetLessThanServerSet) {
  AppsList local_and_server_apps = CreateAppsList("https://example.com/", 10);
  AppsList expected_apps_to_install =
      CreateAppsList("https://example.org/", 10);
  // These fields are not synced, these are just expected values.
  for (std::unique_ptr<WebApp>& expected_app_to_install :
       expected_apps_to_install) {
    expected_app_to_install->SetIsLocallyInstalled(
        AreAppsLocallyInstalledBySync());
    expected_app_to_install->SetIsFromSyncAndPendingInstallation(true);
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, local_and_server_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(expected_apps_to_install, &sync_data_list);
  ConvertAppsListToEntityChangeList(local_and_server_apps, &sync_data_list);

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  base::RunLoop run_loop;
  // This is called after apps are installed from sync in MergeFullSyncData()
  // below.
  sync_bridge().SetInstallWebAppsAfterSyncCallbackForTesting(
      base::BindLambdaForTesting(
          [&](std::vector<WebApp*> apps_to_install,
              WebAppSyncBridge::RepeatingInstallCallback callback) {
            for (WebApp* app_to_install : apps_to_install) {
              // The app must be registered.
              EXPECT_TRUE(registrar().GetAppById(app_to_install->app_id()));
              // Add the app copy to the expected registry.
              registry.emplace(app_to_install->app_id(),
                               std::make_unique<WebApp>(*app_to_install));

              // Find the app in expected_apps_to_install set and remove the
              // entry.
              bool found = false;
              for (const std::unique_ptr<WebApp>& expected_app_to_install :
                   expected_apps_to_install) {
                if (expected_app_to_install->app_id() ==
                    app_to_install->app_id()) {
                  EXPECT_EQ(*expected_app_to_install, *app_to_install);
                  RemoveWebAppFromAppsList(&expected_apps_to_install,
                                           expected_app_to_install->app_id());
                  found = true;
                  break;
                }
              }
              EXPECT_TRUE(found);
            }

            EXPECT_TRUE(expected_apps_to_install.empty());

            RunCallbacksOnInstall(
                apps_to_install, callback,
                webapps::InstallResultCode::kSuccessNewInstall);
            run_loop.Quit();
          }));

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_EmptyEntityChanges) {
  AppsList merged_apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeFullSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;
  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_AddUpdateDelete) {
  // 20 initial apps with DisplayMode::kStandalone user display mode.
  AppsList merged_apps = CreateAppsList("https://example.com/", 20);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeFullSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;

  for (std::unique_ptr<WebApp>& app_to_add :
       CreateAppsList("https://example.org/", 10)) {
    app_to_add->SetIsLocallyInstalled(AreAppsLocallyInstalledBySync());
    app_to_add->SetIsFromSyncAndPendingInstallation(true);

    ConvertAppToEntityChange(*app_to_add, syncer::EntityChange::ACTION_ADD,
                             &entity_changes);
  }

  // Update first 5 initial apps.
  for (int i = 0; i < 5; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*merged_apps[i]);
    // Update user display mode field.
    app_to_update->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    ConvertAppToEntityChange(
        *app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
    // Override the app in the expected registry.
    registry[app_to_update->app_id()] = std::move(app_to_update);
  }

  // Delete next 5 initial apps. Leave the rest unchanged.
  for (int i = 5; i < 10; ++i) {
    const WebApp& app_to_delete = *merged_apps[i];
    ConvertAppToEntityChange(app_to_delete, syncer::EntityChange::ACTION_DELETE,
                             &entity_changes);
  }

  // There should be no changes sent to USS in the next
  // ApplyIncrementalSyncChanges() operation.
  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  sync_bridge().SetInstallWebAppsAfterSyncCallbackForTesting(
      base::BindLambdaForTesting(
          [&](std::vector<WebApp*> apps_to_install,
              WebAppSyncBridge::RepeatingInstallCallback callback) {
            for (WebApp* app_to_install : apps_to_install) {
              // The app must be registered.
              EXPECT_TRUE(registrar().GetAppById(app_to_install->app_id()));
              // Add the app copy to the expected registry.
              registry.emplace(app_to_install->app_id(),
                               std::make_unique<WebApp>(*app_to_install));
            }

            RunCallbacksOnInstall(
                apps_to_install, callback,
                webapps::InstallResultCode::kSuccessNewInstall);
            barrier_closure.Run();
          }));

  sync_bridge().SetUninstallFromSyncCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const std::vector<AppId>& apps_to_uninstall,
              WebAppSyncBridge::RepeatingUninstallCallback callback) {
            EXPECT_EQ(5ul, apps_to_uninstall.size());
            for (const AppId& app_to_uninstall : apps_to_uninstall) {
              // The app must be registered.
              const WebApp* app = registrar().GetAppById(app_to_uninstall);
              // Sync expects that the apps are deleted by the delegate.
              EXPECT_TRUE(app);
              EXPECT_TRUE(app->is_uninstalling());
              EXPECT_TRUE(app->GetSources().Empty());
              registry.erase(app_to_uninstall);
              {
                ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
                update->DeleteApp(app_to_uninstall);
              }
              callback.Run(app_to_uninstall,
                           webapps::UninstallResultCode::kSuccess);
            }

            barrier_closure.Run();
          }));

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_DeleteHappensExternally) {
  // 5 initial apps.
  AppsList merged_apps = CreateAppsList("https://example.com/", 5);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeFullSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;

  // Delete next 5 initial apps. Leave the rest unchanged.
  for (int i = 0; i < 5; ++i) {
    const WebApp& app_to_delete = *merged_apps[i];
    ConvertAppToEntityChange(app_to_delete, syncer::EntityChange::ACTION_DELETE,
                             &entity_changes);
  }

  // There should be no changes sent to USS in the next
  // ApplyIncrementalSyncChanges() operation.
  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);

  base::RunLoop run_loop;
  std::vector<AppId> to_uninstall;
  WebAppSyncBridge::RepeatingUninstallCallback uninstall_complete_callback;

  sync_bridge().SetUninstallFromSyncCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const std::vector<AppId>& apps_to_uninstall,
              WebAppSyncBridge::RepeatingUninstallCallback callback) {
            to_uninstall = apps_to_uninstall;
            uninstall_complete_callback = callback;
            run_loop.Quit();
          }));

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));
  run_loop.Run();

  EXPECT_EQ(5ul, to_uninstall.size());

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
  for (const AppId& app_to_uninstall : to_uninstall) {
    const WebApp* app = registrar().GetAppById(app_to_uninstall);
    EXPECT_TRUE(app);
    EXPECT_TRUE(app->is_uninstalling());
    EXPECT_TRUE(app->GetSources().Empty());
  }
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_UpdateOnly) {
  AppsList merged_apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeFullSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;

  // Update last 5 initial apps.
  for (int i = 5; i < 10; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*merged_apps[i]);
    app_to_update->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Sync Name";
    sync_fallback_data.theme_color = SK_ColorYELLOW;
    app_to_update->SetSyncFallbackData(std::move(sync_fallback_data));

    ConvertAppToEntityChange(
        *app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
    // Override the app in the expected registry.
    registry[app_to_update->app_id()] = std::move(app_to_update);
  }

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);

  // No installs or uninstalls are made here, only app updates.
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_AddSyncAppsWithOverlappingPolicyApps) {
  AppsList policy_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_app = test::CreateWebApp(
        GURL("https://example.com/" + base::NumberToString(i)),
        WebAppManagement::kPolicy);
    policy_apps.push_back(std::move(policy_app));
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, policy_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  syncer::EntityChangeList entity_changes;

  // Install 5 kSync apps over existing kPolicy apps. Leave the rest unchanged.
  for (int i = 0; i < 5; ++i) {
    const WebApp& app_to_install = *policy_apps[i];
    ConvertAppToEntityChange(app_to_install, syncer::EntityChange::ACTION_ADD,
                             &entity_changes);
  }

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<WebApp>& expected_sync_and_policy_app =
        registry[policy_apps[i]->app_id()];
    expected_sync_and_policy_app->AddSource(WebAppManagement::kSync);
  }

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_UpdateSyncAppsWithOverlappingPolicyApps) {
  AppsList policy_and_sync_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_and_sync_app = test::CreateWebApp(
        GURL("https://example.com/" + base::NumberToString(i)));
    policy_and_sync_app->AddSource(WebAppManagement::kPolicy);
    policy_and_sync_app->AddSource(WebAppManagement::kSync);
    policy_and_sync_apps.push_back(std::move(policy_and_sync_app));
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, policy_and_sync_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeFullSyncData(policy_and_sync_apps);

  syncer::EntityChangeList entity_changes;

  // Update first 5 kSync apps which are shared with kPolicy. Leave the rest
  // unchanged.
  AppsList apps_to_update;
  for (int i = 0; i < 5; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*policy_and_sync_apps[i]);
    app_to_update->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Updated Sync Name";
    sync_fallback_data.theme_color = SK_ColorWHITE;
    app_to_update->SetSyncFallbackData(std::move(sync_fallback_data));

    ConvertAppToEntityChange(
        *app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
    apps_to_update.push_back(std::move(app_to_update));
  }

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i)
    registry[policy_and_sync_apps[i]->app_id()] = std::move(apps_to_update[i]);

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

// Tests that if a policy app is installed, and that app is also in 'sync' and
// is uninstalled through sync, then it should remain on the system as a policy
// app.
TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_DeleteSyncAppsWithOverlappingPolicyApps) {
  AppsList policy_and_sync_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_and_sync_app = test::CreateWebApp(
        GURL("https://example.com/" + base::NumberToString(i)));
    policy_and_sync_app->AddSource(WebAppManagement::kPolicy);
    policy_and_sync_app->AddSource(WebAppManagement::kSync);
    policy_and_sync_apps.push_back(std::move(policy_and_sync_app));
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, policy_and_sync_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeFullSyncData(policy_and_sync_apps);

  syncer::EntityChangeList entity_changes;

  // Uninstall 5 kSync apps which are shared with kPolicy. Leave the rest
  // unchanged.
  for (int i = 0; i < 5; ++i) {
    const WebApp& app_to_uninstall = *policy_and_sync_apps[i];
    ConvertAppToEntityChange(
        app_to_uninstall, syncer::EntityChange::ACTION_DELETE, &entity_changes);
  }

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<WebApp>& expected_policy_app =
        registry[policy_and_sync_apps[i]->app_id()];
    expected_policy_app->RemoveSource(WebAppManagement::kSync);
  }

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

// Commits local data (e.g. installed web apps) before sync is hooked up. This
// tests that the web apps are correctly sent to USS after MergeFullSyncData is
// called.
TEST_F(WebAppSyncBridgeTest, CommitUpdate_CommitWhileNotTrackingMetadata) {
  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
  InitSyncBridge();

  AppsList sync_apps = CreateAppsList("https://example.com/", 10);
  Registry expected_registry;
  InsertAppsListIntoRegistry(&expected_registry, sync_apps);

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(processor(), IsTrackingMetadata())
      .WillOnce(testing::Return(false));

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    for (const std::unique_ptr<WebApp>& app : sync_apps) {
      update->CreateApp(std::make_unique<WebApp>(*app));
    }
  }
  EXPECT_TRUE(future.Take());
  testing::Mock::VerifyAndClear(&processor());

  // Do MergeFullSyncData next.
  base::RunLoop run_loop;
  ON_CALL(processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_TRUE(RemoveEntityDataAppFromAppsList(storage_key, *entity_data,
                                                    &sync_apps));
        if (sync_apps.empty())
          run_loop.Quit();
      });

  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(processor(), IsTrackingMetadata())
      .WillOnce(testing::Return(true));

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  syncer::EntityChangeList{});
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), expected_registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, CommitUpdate_CreateSyncApp) {
  InitSyncBridge();

  AppsList sync_apps = CreateAppsList("https://example.com/", 10);
  Registry expected_registry;
  InsertAppsListIntoRegistry(&expected_registry, sync_apps);

  ON_CALL(processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        ASSERT_TRUE(base::Contains(expected_registry, storage_key));
        const std::unique_ptr<WebApp>& expected_app =
            expected_registry.at(storage_key);
        EXPECT_TRUE(IsSyncDataEqual(*expected_app, *entity_data));
        RemoveWebAppFromAppsList(&sync_apps, storage_key);
      });
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(processor(), IsTrackingMetadata())
      .WillOnce(testing::Return(true));

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    for (const std::unique_ptr<WebApp>& app : sync_apps) {
      update->CreateApp(std::make_unique<WebApp>(*app));
    }
  }
  EXPECT_TRUE(future.Take());

  EXPECT_TRUE(sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), expected_registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, CommitUpdate_UpdateSyncApp) {
  AppsList sync_apps = CreateAppsList("https://example.com/", 10);
  Registry registry;
  InsertAppsListIntoRegistry(&registry, sync_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  ON_CALL(processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        ASSERT_TRUE(base::Contains(registry, storage_key));
        const std::unique_ptr<WebApp>& expected_app = registry.at(storage_key);
        EXPECT_TRUE(IsSyncDataEqual(*expected_app, *entity_data));
        RemoveWebAppFromAppsList(&sync_apps, storage_key);
      });
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    for (const std::unique_ptr<WebApp>& app : sync_apps) {
      // Obtain a writeable handle.
      WebApp* sync_app = update->UpdateApp(app->app_id());

      WebApp::SyncFallbackData sync_fallback_data;
      sync_fallback_data.name = "Updated Sync Name";
      sync_fallback_data.theme_color = SK_ColorBLACK;
      sync_app->SetSyncFallbackData(std::move(sync_fallback_data));
      sync_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);

      // Override the app in the expected registry.
      registry[sync_app->app_id()] = std::make_unique<WebApp>(*sync_app);
    }
  }
  EXPECT_TRUE(future.Take());

  EXPECT_TRUE(sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, CommitUpdate_DeleteSyncApp) {
  AppsList sync_apps = CreateAppsList("https://example.com/", 10);
  Registry registry;
  InsertAppsListIntoRegistry(&registry, sync_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  ON_CALL(processor(), Delete(_, _))
      .WillByDefault([&](const std::string& storage_key,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_TRUE(base::Contains(registry, storage_key));
        RemoveWebAppFromAppsList(&sync_apps, storage_key);
        // Delete the app in the expected registry.
        registry.erase(storage_key);
      });

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    for (const std::unique_ptr<WebApp>& app : sync_apps) {
      update->DeleteApp(app->app_id());
    }
  }
  EXPECT_TRUE(future.Take());

  EXPECT_TRUE(sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       CommitUpdate_CreateSyncAppWithOverlappingPolicyApp) {
  AppsList policy_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_app = test::CreateWebApp(
        GURL("https://example.com/" + base::NumberToString(i)),
        WebAppManagement::kPolicy);
    policy_apps.push_back(std::move(policy_app));
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, policy_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  ON_CALL(processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        ASSERT_TRUE(base::Contains(registry, storage_key));
        const WebApp& expected_app = *registry.at(storage_key);

        // kPolicy and Name is the difference for the sync "view". Add them to
        // make operator== work.
        std::unique_ptr<WebApp> entity_data_app =
            std::make_unique<WebApp>(expected_app.app_id());
        entity_data_app->AddSource(WebAppManagement::kPolicy);
        entity_data_app->SetName("Name");
        entity_data_app->SetIsLocallyInstalled(true);

        EXPECT_TRUE(IsSyncDataEqualIfApplied(
            expected_app, std::move(entity_data_app), *entity_data));

        RemoveWebAppFromAppsList(&policy_apps, storage_key);
      });
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    for (int i = 0; i < 10; ++i) {
      WebApp* app_to_update = update->UpdateApp(policy_apps[i]->app_id());

      // Add kSync source to first 5 apps. Modify the rest 5 apps locally.
      if (i < 5) {
        app_to_update->AddSource(WebAppManagement::kSync);
      } else {
        app_to_update->SetDescription("Local policy app");
      }

      // Override the app in the expected registry.
      registry[app_to_update->app_id()] =
          std::make_unique<WebApp>(*app_to_update);
    }
  }
  EXPECT_TRUE(future.Take());

  EXPECT_EQ(5u, policy_apps.size());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       CommitUpdate_DeleteSyncAppWithOverlappingPolicyApp) {
  AppsList policy_and_sync_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_and_sync_app = test::CreateWebApp(
        GURL("https://example.com/" + base::NumberToString(i)));
    policy_and_sync_app->AddSource(WebAppManagement::kPolicy);
    policy_and_sync_app->AddSource(WebAppManagement::kSync);
    policy_and_sync_apps.push_back(std::move(policy_and_sync_app));
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, policy_and_sync_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  ON_CALL(processor(), Put(_, _, _))
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        // Local changes to synced apps cause excessive |Put|.
        // See TODO in WebAppSyncBridge::UpdateSync.
        RemoveWebAppFromAppsList(&policy_and_sync_apps, storage_key);
      });
  ON_CALL(processor(), Delete(_, _))
      .WillByDefault([&](const std::string& storage_key,
                         syncer::MetadataChangeList* metadata) {
        ASSERT_TRUE(base::Contains(registry, storage_key));
        RemoveWebAppFromAppsList(&policy_and_sync_apps, storage_key);
      });

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    for (int i = 0; i < 10; ++i) {
      WebApp* app_to_update =
          update->UpdateApp(policy_and_sync_apps[i]->app_id());

      // Remove kSync source from first 5 apps. Modify the rest 5 apps locally.
      if (i < 5) {
        app_to_update->RemoveSource(WebAppManagement::kSync);
      } else {
        app_to_update->SetDescription("Local policy app");
      }

      // Override the app in the expected registry.
      registry[app_to_update->app_id()] =
          std::make_unique<WebApp>(*app_to_update);
    }
  }
  EXPECT_TRUE(future.Take());

  EXPECT_TRUE(policy_and_sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

// Test that any apps that are still pending install from sync (or,
// |is_from_sync_and_pending_installation|) are continued to be installed when
// the bridge initializes.
TEST_F(WebAppSyncBridgeTest, InstallAppsFromSyncAndPendingInstallation) {
  AppsList apps_in_sync_install = CreateAppsList("https://example.com/", 10);
  for (std::unique_ptr<WebApp>& app : apps_in_sync_install) {
    app->SetIsLocallyInstalled(AreAppsLocallyInstalledBySync());
    app->SetIsFromSyncAndPendingInstallation(true);
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, apps_in_sync_install);
  database_factory().WriteRegistry(registry);

  base::RunLoop run_loop;
  sync_bridge().SetInstallWebAppsAfterSyncCallbackForTesting(
      base::BindLambdaForTesting(
          [&](std::vector<WebApp*> apps_to_install,
              WebAppSyncBridge::RepeatingInstallCallback callback) {
            for (WebApp* app_to_install : apps_to_install) {
              // The app must be registered.
              EXPECT_TRUE(registrar().GetAppById(app_to_install->app_id()));
              RemoveWebAppFromAppsList(&apps_in_sync_install,
                                       app_to_install->app_id());
            }

            EXPECT_TRUE(apps_in_sync_install.empty());
            RunCallbacksOnInstall(
                apps_to_install, callback,
                webapps::InstallResultCode::kSuccessNewInstall);
            run_loop.Quit();
          }));

  InitSyncBridge();

  run_loop.Run();
}

// Tests that OnWebAppsWillBeUpdatedFromSync observer notification is called
// properly.
TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_OnWebAppsWillBeUpdatedFromSync) {
  AppsList initial_registry_apps = CreateAppsList("https://example.com/", 10);
  for (std::unique_ptr<WebApp>& app : initial_registry_apps)
    app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  InitSyncBridgeFromAppList(initial_registry_apps);

  WebAppTestRegistryObserverAdapter observer{&registrar()};
  base::RunLoop run_loop;

  observer.SetWebAppWillBeUpdatedFromSyncDelegate(base::BindLambdaForTesting(
      [&](const std::vector<const WebApp*>& new_apps_state) {
        EXPECT_EQ(5u, new_apps_state.size());

        for (const WebApp* new_app_state : new_apps_state) {
          const WebApp* old_app_state =
              registrar().GetAppById(new_app_state->app_id());
          EXPECT_NE(*old_app_state, *new_app_state);

          EXPECT_EQ(old_app_state->user_display_mode(),
                    mojom::UserDisplayMode::kBrowser);
          EXPECT_EQ(new_app_state->user_display_mode(),
                    mojom::UserDisplayMode::kStandalone);

          // new and old states must be equal if diff fixed:
          auto old_app_state_no_diff = std::make_unique<WebApp>(*old_app_state);
          old_app_state_no_diff->SetUserDisplayMode(
              mojom::UserDisplayMode::kStandalone);
          EXPECT_EQ(*old_app_state_no_diff, *new_app_state);

          RemoveWebAppFromAppsList(&initial_registry_apps,
                                   new_app_state->app_id());
        }

        run_loop.Quit();
      }));

  AppsList apps_server_state;

  // Update first 5 apps: change user_display_mode field only.
  for (int i = 0; i < 5; ++i) {
    auto app_server_state = std::make_unique<WebApp>(*initial_registry_apps[i]);
    app_server_state->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
    apps_server_state.push_back(std::move(app_server_state));
  }

  sync_bridge_test_utils::UpdateApps(sync_bridge(), apps_server_state);

  run_loop.Run();

  // 5 other apps left unchanged:
  EXPECT_EQ(5u, initial_registry_apps.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
              initial_registry_apps[i]->user_display_mode());
  }
}

TEST_F(WebAppSyncBridgeTest, RetryIncompleteUninstalls) {
  AppsList initial_registry_apps = CreateAppsList("https://example.com/", 5);
  std::vector<AppId> initial_app_ids;
  for (std::unique_ptr<WebApp>& app : initial_registry_apps) {
    app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    app->SetIsUninstalling(true);
    initial_app_ids.push_back(app->app_id());
  }

  SetSyncInstallCallbackFailureIfCalled();

  base::RunLoop run_loop;
  sync_bridge().SetRetryIncompleteUninstallsCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const base::flat_set<AppId>& apps_to_uninstall) {
            EXPECT_EQ(apps_to_uninstall.size(), 5ul);
            EXPECT_THAT(apps_to_uninstall, ::testing::UnorderedElementsAreArray(
                                               apps_to_uninstall));
            run_loop.Quit();
          }));

  InitSyncBridgeFromAppList(initial_registry_apps);

  run_loop.Run();
}

}  // namespace web_app
