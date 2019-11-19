// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
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

  const GURL sync_launch_url(entity_data.specifics.web_app().launch_url());
  if (expected_app.app_id() != GenerateAppIdFromURL(sync_launch_url))
    return false;

  // ApplySyncDataToApp enforces kSync source on |app_to_apply_sync_data|.
  ApplySyncDataToApp(entity_data.specifics.web_app(),
                     app_to_apply_sync_data.get());
  return expected_app == *app_to_apply_sync_data;
}

bool IsSyncDataEqual(const WebApp& expected_app,
                     const syncer::EntityData& entity_data) {
  auto app_to_apply_sync_data = std::make_unique<WebApp>(expected_app.app_id());
  return IsSyncDataEqualIfApplied(
      expected_app, std::move(app_to_apply_sync_data), entity_data);
}

bool SyncDataBatchMatchesRegistry(
    const Registry& registry,
    std::unique_ptr<syncer::DataBatch> data_batch) {
  if (!data_batch || !data_batch->HasNext())
    return false;

  syncer::KeyAndData key_and_data1 = data_batch->Next();
  if (!IsSyncDataEqual(*registry.at(key_and_data1.first),
                       *key_and_data1.second)) {
    return false;
  }

  if (!data_batch->HasNext())
    return false;

  syncer::KeyAndData key_and_data2 = data_batch->Next();
  if (!IsSyncDataEqual(*registry.at(key_and_data2.first),
                       *key_and_data2.second)) {
    return false;
  }

  return !data_batch->HasNext();
}

std::unique_ptr<WebApp> CreateWebApp(const std::string& url) {
  const GURL launch_url(url);
  const AppId app_id = GenerateAppIdFromURL(launch_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetLaunchUrl(launch_url);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetName("Name");
  return web_app;
}

std::unique_ptr<WebApp> CreateWebAppWithSyncOnlyFields(const std::string& url) {
  const GURL launch_url(url);
  const AppId app_id = GenerateAppIdFromURL(launch_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(Source::kSync);
  web_app->SetLaunchUrl(launch_url);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
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
          app.app_id(), CreateSyncEntityData(app));
      break;
    case syncer::EntityChange::ACTION_UPDATE:
      entity_change = syncer::EntityChange::CreateUpdate(
          app.app_id(), CreateSyncEntityData(app));
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
    TestWebAppRegistryController::RepeatingInstallCallback callback,
    InstallResultCode code) {
  for (WebApp* app : apps)
    callback.Run(app->app_id(), code);
}

void RunCallbacksOnUninstall(
    const std::vector<std::unique_ptr<WebApp>>& apps,
    TestWebAppRegistryController::RepeatingUninstallCallback callback,
    bool uninstalled) {
  for (const std::unique_ptr<WebApp>& app : apps)
    callback.Run(app->app_id(), uninstalled);
}

}  // namespace

class WebAppSyncBridgeTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());
  }

  void TearDown() override {
    test_registry_controller_.reset();

    WebAppTest::TearDown();
  }

  void InitSyncBridge() { controller().Init(); }

  void MergeSyncData(const AppsList& merged_apps) {
    syncer::EntityChangeList entity_data_list;
    ConvertAppsListToEntityChangeList(merged_apps, &entity_data_list);
    EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
    EXPECT_CALL(processor(), Delete(_, _)).Times(0);
    sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
                                std::move(entity_data_list));
  }

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(registrar_registry(), registry);
  }

  void SetSyncInstallDelegateFailureIfCalled() {
    controller().SetInstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
        [&](std::vector<WebApp*> apps_to_install,
            TestWebAppRegistryController::RepeatingInstallCallback callback) {
          ADD_FAILURE();
        }));

    controller().SetUninstallWebAppsAfterSyncDelegate(
        base::BindLambdaForTesting(
            [&](std::vector<std::unique_ptr<WebApp>> apps_to_uninstall,
                TestWebAppRegistryController::RepeatingUninstallCallback
                    callback) { ADD_FAILURE(); }));
  }

  void CommitUpdate(std::unique_ptr<WebAppRegistryUpdate> update) {
    base::RunLoop run_loop;
    sync_bridge().CommitUpdate(
        std::move(update),
        base::BindLambdaForTesting([&run_loop](bool success) {
          ASSERT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

 protected:
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }
  syncer::MockModelTypeChangeProcessor& processor() {
    return controller().processor();
  }
  TestWebAppDatabaseFactory& database_factory() {
    return controller().database_factory();
  }
  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }
  WebAppRegistrar& registrar() { return controller().mutable_registrar(); }
  Registry& registrar_registry() {
    return controller().mutable_registrar().registry();
  }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
};

TEST_F(WebAppSyncBridgeTest, GetData) {
  Registry registry;

  std::unique_ptr<WebApp> synced_app1 =
      CreateWebAppWithSyncOnlyFields("https://example.com/app1/");
  {
    WebApp::SyncData sync_data;
    sync_data.name = "Sync Name";
    sync_data.theme_color = SK_ColorCYAN;
    synced_app1->SetSyncData(std::move(sync_data));
  }
  InsertAppIntoRegistry(&registry, std::move(synced_app1));

  std::unique_ptr<WebApp> synced_app2 =
      CreateWebAppWithSyncOnlyFields("https://example.com/app2/");
  // sync_data is empty for this app.
  InsertAppIntoRegistry(&registry, std::move(synced_app2));

  std::unique_ptr<WebApp> policy_app = CreateWebApp("https://example.org/");
  policy_app->AddSource(Source::kPolicy);
  InsertAppIntoRegistry(&registry, std::move(policy_app));

  database_factory().WriteRegistry(registry);

  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
  InitSyncBridge();

  {
    WebAppSyncBridge::StorageKeyList storage_keys;
    storage_keys.push_back("unknown");
    for (const Registry::value_type& id_and_web_app : registry)
      storage_keys.push_back(id_and_web_app.first);

    base::RunLoop run_loop;
    sync_bridge().GetData(
        std::move(storage_keys),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<syncer::DataBatch> data_batch) {
              EXPECT_TRUE(SyncDataBatchMatchesRegistry(registry,
                                                       std::move(data_batch)));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    sync_bridge().GetAllDataForDebugging(base::BindLambdaForTesting(
        [&](std::unique_ptr<syncer::DataBatch> data_batch) {
          EXPECT_TRUE(
              SyncDataBatchMatchesRegistry(registry, std::move(data_batch)));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(WebAppSyncBridgeTest, Identities) {
  std::unique_ptr<WebApp> app =
      CreateWebAppWithSyncOnlyFields("https://example.com/");
  std::unique_ptr<syncer::EntityData> entity_data = CreateSyncEntityData(*app);

  EXPECT_EQ(app->app_id(), sync_bridge().GetClientTag(*entity_data));
  EXPECT_EQ(app->app_id(), sync_bridge().GetStorageKey(*entity_data));
}

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetAndServerSetAreEmpty) {
  InitSyncBridge();

  syncer::EntityChangeList sync_data_list;

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
                              std::move(sync_data_list));
}

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetEqualsServerSet) {
  AppsList apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();

  // The incoming list of apps from the sync server.
  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(apps, &sync_data_list);

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);

  sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
                              std::move(sync_data_list));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetGreaterThanServerSet) {
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

  sync_bridge().MergeSyncData(std::move(metadata_change_list),
                              std::move(sync_data_list));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, MergeSyncData_LocalSetLessThanServerSet) {
  AppsList local_and_server_apps = CreateAppsList("https://example.com/", 10);
  AppsList expected_apps_to_install =
      CreateAppsList("https://example.org/", 10);
  // These fields are not synced, these are just expected values.
  for (std::unique_ptr<WebApp>& expected_app_to_install :
       expected_apps_to_install) {
    expected_app_to_install->SetIsLocallyInstalled(
        AreAppsLocallyInstalledByDefault());
    expected_app_to_install->SetIsInSyncInstall(true);
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
  controller().SetInstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
      [&](std::vector<WebApp*> apps_to_install,
          TestWebAppRegistryController::RepeatingInstallCallback callback) {
        for (WebApp* app_to_install : apps_to_install) {
          // The app must be registered.
          EXPECT_TRUE(registrar().GetAppById(app_to_install->app_id()));
          // Add the app copy to the expected registry.
          registry.emplace(app_to_install->app_id(),
                           std::make_unique<WebApp>(*app_to_install));

          // Find the app in expected_apps_to_install set and remove the entry.
          bool found = false;
          for (const std::unique_ptr<WebApp>& expected_app_to_install :
               expected_apps_to_install) {
            if (expected_app_to_install->app_id() == app_to_install->app_id()) {
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

        RunCallbacksOnInstall(apps_to_install, callback,
                              InstallResultCode::kSuccessNewInstall);
        run_loop.Quit();
      }));

  sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
                              std::move(sync_data_list));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, ApplySyncChanges_EmptyEntityChanges) {
  AppsList merged_apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;
  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  SetSyncInstallDelegateFailureIfCalled();

  sync_bridge().ApplySyncChanges(sync_bridge().CreateMetadataChangeList(),
                                 std::move(entity_changes));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, ApplySyncChanges_AddUpdateDelete) {
  // 20 initial apps with DisplayMode::kStandalone user display mode.
  AppsList merged_apps = CreateAppsList("https://example.com/", 20);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;

  for (std::unique_ptr<WebApp>& app_to_add :
       CreateAppsList("https://example.org/", 10)) {
    app_to_add->SetIsLocallyInstalled(AreAppsLocallyInstalledByDefault());
    app_to_add->SetIsInSyncInstall(true);

    ConvertAppToEntityChange(*app_to_add, syncer::EntityChange::ACTION_ADD,
                             &entity_changes);
  }

  // Update first 5 initial apps.
  for (int i = 0; i < 5; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*merged_apps[i]);
    // Update user display mode field.
    app_to_update->SetUserDisplayMode(DisplayMode::kBrowser);
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

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  controller().SetInstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
      [&](std::vector<WebApp*> apps_to_install,
          TestWebAppRegistryController::RepeatingInstallCallback callback) {
        for (WebApp* app_to_install : apps_to_install) {
          // The app must be registered.
          EXPECT_TRUE(registrar().GetAppById(app_to_install->app_id()));
          // Add the app copy to the expected registry.
          registry.emplace(app_to_install->app_id(),
                           std::make_unique<WebApp>(*app_to_install));
        }

        RunCallbacksOnInstall(apps_to_install, callback,
                              InstallResultCode::kSuccessNewInstall);
        barrier_closure.Run();
      }));

  controller().SetUninstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<WebApp>> apps_to_uninstall,
          TestWebAppRegistryController::RepeatingUninstallCallback callback) {
        for (std::unique_ptr<WebApp>& app_to_uninstall : apps_to_uninstall) {
          // The app must be unregistered.
          EXPECT_FALSE(registrar().GetAppById(app_to_uninstall->app_id()));
          registry.erase(app_to_uninstall->app_id());
        }

        RunCallbacksOnUninstall(apps_to_uninstall, callback,
                                /*uninstalled=*/true);
        barrier_closure.Run();
      }));

  sync_bridge().ApplySyncChanges(sync_bridge().CreateMetadataChangeList(),
                                 std::move(entity_changes));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, ApplySyncChanges_UpdateOnly) {
  AppsList merged_apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;

  // Update last 5 initial apps.
  for (int i = 5; i < 10; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*merged_apps[i]);
    app_to_update->SetUserDisplayMode(DisplayMode::kStandalone);

    WebApp::SyncData sync_data;
    sync_data.name = "Sync Name";
    sync_data.theme_color = SK_ColorYELLOW;
    app_to_update->SetSyncData(std::move(sync_data));

    ConvertAppToEntityChange(
        *app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
    // Override the app in the expected registry.
    registry[app_to_update->app_id()] = std::move(app_to_update);
  }

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  SetSyncInstallDelegateFailureIfCalled();

  sync_bridge().ApplySyncChanges(sync_bridge().CreateMetadataChangeList(),
                                 std::move(entity_changes));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       ApplySyncChanges_AddSyncAppsWithOverlappingPolicyApps) {
  AppsList policy_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_app =
        CreateWebApp("https://example.com/" + base::NumberToString(i));
    policy_app->AddSource(Source::kPolicy);
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
  SetSyncInstallDelegateFailureIfCalled();

  sync_bridge().ApplySyncChanges(sync_bridge().CreateMetadataChangeList(),
                                 std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<WebApp>& expected_sync_and_policy_app =
        registry[policy_apps[i]->app_id()];
    expected_sync_and_policy_app->AddSource(Source::kSync);
  }

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       ApplySyncChanges_UpdateSyncAppsWithOverlappingPolicyApps) {
  AppsList policy_and_sync_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_and_sync_app =
        CreateWebApp("https://example.com/" + base::NumberToString(i));
    policy_and_sync_app->AddSource(Source::kPolicy);
    policy_and_sync_app->AddSource(Source::kSync);
    policy_and_sync_apps.push_back(std::move(policy_and_sync_app));
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, policy_and_sync_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeSyncData(policy_and_sync_apps);

  syncer::EntityChangeList entity_changes;

  // Update first 5 kSync apps which are shared with kPolicy. Leave the rest
  // unchanged.
  AppsList apps_to_update;
  for (int i = 0; i < 5; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*policy_and_sync_apps[i]);
    app_to_update->SetUserDisplayMode(DisplayMode::kBrowser);

    WebApp::SyncData sync_data;
    sync_data.name = "Updated Sync Name";
    sync_data.theme_color = SK_ColorWHITE;
    app_to_update->SetSyncData(std::move(sync_data));

    ConvertAppToEntityChange(
        *app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
    apps_to_update.push_back(std::move(app_to_update));
  }

  EXPECT_CALL(processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);
  SetSyncInstallDelegateFailureIfCalled();

  sync_bridge().ApplySyncChanges(sync_bridge().CreateMetadataChangeList(),
                                 std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i)
    registry[policy_and_sync_apps[i]->app_id()] = std::move(apps_to_update[i]);

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       ApplySyncChanges_DeleteSyncAppsWithOverlappingPolicyApps) {
  AppsList policy_and_sync_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_and_sync_app =
        CreateWebApp("https://example.com/" + base::NumberToString(i));
    policy_and_sync_app->AddSource(Source::kPolicy);
    policy_and_sync_app->AddSource(Source::kSync);
    policy_and_sync_apps.push_back(std::move(policy_and_sync_app));
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, policy_and_sync_apps);
  database_factory().WriteRegistry(registry);
  InitSyncBridge();
  MergeSyncData(policy_and_sync_apps);

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
  SetSyncInstallDelegateFailureIfCalled();

  sync_bridge().ApplySyncChanges(sync_bridge().CreateMetadataChangeList(),
                                 std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<WebApp>& expected_policy_app =
        registry[policy_and_sync_apps[i]->app_id()];
    expected_policy_app->RemoveSource(Source::kSync);
  }

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

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

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

  for (const std::unique_ptr<WebApp>& app : sync_apps)
    update->CreateApp(std::make_unique<WebApp>(*app));

  CommitUpdate(std::move(update));
  testing::Mock::VerifyAndClear(&processor());

  // Do MergeSyncData next.
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

  sync_bridge().MergeSyncData(sync_bridge().CreateMetadataChangeList(),
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

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

  for (const std::unique_ptr<WebApp>& app : sync_apps)
    update->CreateApp(std::make_unique<WebApp>(*app));

  CommitUpdate(std::move(update));

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

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

  for (const std::unique_ptr<WebApp>& app : sync_apps) {
    // Obtain a writeable handle.
    WebApp* sync_app = update->UpdateApp(app->app_id());

    WebApp::SyncData sync_data;
    sync_data.name = "Updated Sync Name";
    sync_data.theme_color = SK_ColorBLACK;
    sync_app->SetSyncData(std::move(sync_data));
    sync_app->SetUserDisplayMode(DisplayMode::kBrowser);

    // Override the app in the expected registry.
    registry[sync_app->app_id()] = std::make_unique<WebApp>(*sync_app);
  }

  CommitUpdate(std::move(update));

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

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

  for (const std::unique_ptr<WebApp>& app : sync_apps)
    update->DeleteApp(app->app_id());

  CommitUpdate(std::move(update));

  EXPECT_TRUE(sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       CommitUpdate_CreateSyncAppWithOverlappingPolicyApp) {
  AppsList policy_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_app =
        CreateWebApp("https://example.com/" + base::NumberToString(i));
    // CreateWebApp does policy_app->SetName("Name");
    policy_app->AddSource(Source::kPolicy);
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
        entity_data_app->AddSource(Source::kPolicy);
        entity_data_app->SetName("Name");

        EXPECT_TRUE(IsSyncDataEqualIfApplied(
            expected_app, std::move(entity_data_app), *entity_data));

        RemoveWebAppFromAppsList(&policy_apps, storage_key);
      });
  EXPECT_CALL(processor(), Delete(_, _)).Times(0);

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

  for (int i = 0; i < 10; ++i) {
    WebApp* app_to_update = update->UpdateApp(policy_apps[i]->app_id());

    // Add kSync source to first 5 apps. Modify the rest 5 apps locally.
    if (i < 5)
      app_to_update->AddSource(Source::kSync);
    else
      app_to_update->SetDescription("Local policy app");

    // Override the app in the expected registry.
    registry[app_to_update->app_id()] =
        std::make_unique<WebApp>(*app_to_update);
  }

  CommitUpdate(std::move(update));

  EXPECT_EQ(5u, policy_apps.size());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest,
       CommitUpdate_DeleteSyncAppWithOverlappingPolicyApp) {
  AppsList policy_and_sync_apps;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<WebApp> policy_and_sync_app =
        CreateWebApp("https://example.com/" + base::NumberToString(i));
    // CreateWebApp does policy_app->SetName("Name");
    policy_and_sync_app->AddSource(Source::kPolicy);
    policy_and_sync_app->AddSource(Source::kSync);
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

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

  for (int i = 0; i < 10; ++i) {
    WebApp* app_to_update =
        update->UpdateApp(policy_and_sync_apps[i]->app_id());

    // Remove kSync source from first 5 apps. Modify the rest 5 apps locally.
    if (i < 5)
      app_to_update->RemoveSource(Source::kSync);
    else
      app_to_update->SetDescription("Local policy app");

    // Override the app in the expected registry.
    registry[app_to_update->app_id()] =
        std::make_unique<WebApp>(*app_to_update);
  }

  CommitUpdate(std::move(update));

  EXPECT_TRUE(policy_and_sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppSyncBridgeTest, InstallAppsInSyncInstall) {
  AppsList apps_in_sync_install = CreateAppsList("https://example.com/", 10);
  for (std::unique_ptr<WebApp>& app : apps_in_sync_install) {
    app->SetIsLocallyInstalled(AreAppsLocallyInstalledByDefault());
    app->SetIsInSyncInstall(true);
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, apps_in_sync_install);
  database_factory().WriteRegistry(registry);

  base::RunLoop run_loop;
  controller().SetInstallWebAppsAfterSyncDelegate(base::BindLambdaForTesting(
      [&](std::vector<WebApp*> apps_to_install,
          TestWebAppRegistryController::RepeatingInstallCallback callback) {
        for (WebApp* app_to_install : apps_to_install) {
          // The app must be registered.
          EXPECT_TRUE(registrar().GetAppById(app_to_install->app_id()));
          RemoveWebAppFromAppsList(&apps_in_sync_install,
                                   app_to_install->app_id());
        }

        EXPECT_TRUE(apps_in_sync_install.empty());
        RunCallbacksOnInstall(apps_to_install, callback,
                              InstallResultCode::kSuccessNewInstall);
        run_loop.Quit();
      }));

  InitSyncBridge();

  run_loop.Run();
}

}  // namespace web_app
