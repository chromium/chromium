// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_sync_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

using testing::_;

using AppsList = std::vector<std::unique_ptr<WebApp>>;

using mojom::UserDisplayMode;
using sync_pb::WebAppSpecifics_UserDisplayMode;
using sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER;
using sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE;
using sync_pb::WebAppSpecifics_UserDisplayMode_TABBED;
using sync_pb::WebAppSpecifics_UserDisplayMode_UNSPECIFIED;

void RemoveWebAppFromAppsList(AppsList* apps_list,
                              const webapps::AppId& app_id) {
  std::erase_if(*apps_list, [app_id](const std::unique_ptr<WebApp>& app) {
    return app->app_id() == app_id;
  });
}

bool IsSyncDataEqualIfApplied(const WebApp& expected_app,
                              std::unique_ptr<WebApp> app_to_apply_sync_data,
                              const syncer::EntityData& entity_data) {
  if (!entity_data.specifics.has_web_app())
    return false;

  auto& web_app_specifics = entity_data.specifics.web_app();
  const GURL sync_start_url(web_app_specifics.start_url());
  webapps::ManifestId sync_manifest_id = GenerateManifestId(
      web_app_specifics.relative_manifest_id(), sync_start_url);
  if (expected_app.app_id() != GenerateAppIdFromManifestId(sync_manifest_id)) {
    return false;
  }

  // ApplySyncDataToApp does not set the start_url or manifest_id.
  app_to_apply_sync_data->SetStartUrl(sync_start_url);
  app_to_apply_sync_data->SetManifestId(sync_manifest_id);

  // ApplySyncDataToApp enforces kSync source on |app_to_apply_sync_data|.
  ApplySyncDataToApp(entity_data.specifics.web_app(),
                     app_to_apply_sync_data.get());
  app_to_apply_sync_data->SetName(entity_data.name);

  // Remove OS integration state, as this is never applied with the sync system.
  WebApp expected_app_copy = WebApp(expected_app);
  expected_app_copy.SetCurrentOsIntegrationStates(
      proto::WebAppOsIntegrationState());

  WebApp app_applied_sync_data_copy = WebApp(*app_to_apply_sync_data);
  app_applied_sync_data_copy.SetCurrentOsIntegrationStates(
      proto::WebAppOsIntegrationState());

  // The same as what applies to OS integration state also applies to the
  // "user installed" installation source. So remove that as well.
  // TODO(https://crbug.com/372062068): Figure out a better way to handle
  // differences in installed state.
  expected_app_copy.RemoveSource(WebAppManagement::kUserInstalled);
  app_applied_sync_data_copy.RemoveSource(WebAppManagement::kUserInstalled);

  return expected_app_copy == app_applied_sync_data_copy;
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

std::unique_ptr<WebApp> CreateWebAppWithSyncOnlyFields(
    const std::string& url,
    std::optional<std::string> relative_manifest_id = std::nullopt) {
  const GURL start_url(url);
  const webapps::AppId app_id = GenerateAppId(relative_manifest_id, start_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(WebAppManagement::kSync);
  web_app->SetStartUrl(start_url);
  web_app->SetName("Name");
  web_app->SetUserDisplayMode(UserDisplayMode::kStandalone);
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
  webapps::AppId app_id = app->app_id();
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

syncer::EntityChangeList ToEntityChangeList(
    const webapps::AppId& app_id,
    const sync_pb::WebAppSpecifics& sync_proto,
    WebAppSyncBridge& sync_bridge) {
  syncer::EntityChangeList entity_change_list;
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_web_app() = sync_proto;
  CHECK(sync_bridge.IsEntityDataValid(entity_data));
  std::string storage_key = sync_bridge.GetClientTag(entity_data);
  DCHECK_EQ(storage_key, app_id);
  entity_change_list.push_back(
      syncer::EntityChange::CreateUpdate(storage_key, std::move(entity_data)));
  return entity_change_list;
}

}  // namespace

// TODO(dmurph): Replace these tests with tests in SingleClientWebAppsSyncTest,
// which can fake out the sync server and allow the WebAppProvider system to run
// in full w/o mocks.
class WebAppSyncBridgeTest : public WebAppTest {
 public:
  void StartWebAppProvider() {
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void StartWebAppProviderFromAppList(const AppsList& apps_list) {
    Registry registry;
    InsertAppsListIntoRegistry(&registry, apps_list);
    database_factory().WriteRegistry(registry);
    StartWebAppProvider();
  }

  void MergeFullSyncData(const AppsList& merged_apps) {
    syncer::EntityChangeList entity_data_list;
    ConvertAppsListToEntityChangeList(merged_apps, &entity_data_list);
    EXPECT_CALL(processor(), Put).Times(0);
    EXPECT_CALL(processor(), Delete).Times(0);
    sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                    std::move(entity_data_list));
  }

  bool IsDatabaseRegistryEqualToRegistrar(bool exclude_current_os_integration) {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(registrar_registry(), registry,
                           exclude_current_os_integration);
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
            [&](const std::vector<webapps::AppId>& apps_to_uninstall,
                WebAppSyncBridge::RepeatingUninstallCallback callback) {
              ADD_FAILURE();
            }));
  }

 protected:
  syncer::MockDataTypeLocalChangeProcessor& processor() {
    return fake_provider().processor();
  }
  FakeWebAppDatabaseFactory& database_factory() {
    return static_cast<FakeWebAppDatabaseFactory&>(
        fake_provider().GetDatabaseFactory());
  }

  WebAppRegistrar& registrar() { return fake_provider().registrar_unsafe(); }

  WebAppSyncBridge& sync_bridge() {
    return fake_provider().sync_bridge_unsafe();
  }

  WebAppInstallManager& install_manager() {
    return fake_provider().install_manager();
  }

  const Registry& registrar_registry() {
    return fake_provider().registrar_unsafe().registry_for_testing();
  }
};

// Tests that the WebAppSyncBridge correctly reports data from the
// WebAppDatabase.
TEST_F(WebAppSyncBridgeTest, GetData) {
  Registry registry;

  std::unique_ptr<WebApp> synced_app1 =
      CreateWebAppWithSyncOnlyFields("https://example.com/app1/");
  {
    sync_pb::WebAppSpecifics sync_data;
    sync_data.set_name("Sync Name");
    sync_data.set_theme_color(SK_ColorCYAN);
    synced_app1->SetSyncProto(std::move(sync_data));
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
  StartWebAppProvider();

  {
    WebAppSyncBridge::StorageKeyList storage_keys;
    // Add an unknown key to test this is handled gracefully.
    storage_keys.push_back("unknown");
    for (const Registry::value_type& id_and_web_app : registry)
      storage_keys.push_back(id_and_web_app.first);

    EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
        registry, sync_bridge().GetDataForCommit(std::move(storage_keys))));
  }

  EXPECT_TRUE(RegistryContainsSyncDataBatchChanges(
      registry, sync_bridge().GetAllDataForDebugging()));
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
  StartWebAppProvider();

  syncer::EntityChangeList sync_data_list;

  EXPECT_CALL(processor(), Put).Times(0);

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetEqualsServerSet) {
  AppsList apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();

  // The incoming list of apps from the sync server.
  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(apps, &sync_data_list);

  // The local app state is the same as the server state, so no changes should
  // be sent.
  EXPECT_CALL(processor(), Put).Times(0);

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetGreaterThanServerSet) {
  AppsList local_and_server_apps = CreateAppsList("https://example.com/", 10);
  AppsList expected_local_apps_to_upload =
      CreateAppsList("https://example.org/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, local_and_server_apps);
  InsertAppsListIntoRegistry(&registry, expected_local_apps_to_upload);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();

  auto metadata_change_list = sync_bridge().CreateMetadataChangeList();
  syncer::MetadataChangeList* metadata_ptr = metadata_change_list.get();

  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(local_and_server_apps, &sync_data_list);

  // MergeFullSyncData below should send |expected_local_apps_to_upload| to the
  // processor() to upload to USS.
  base::RunLoop run_loop;
  ON_CALL(processor(), Put)
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

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetLessThanServerSet) {
  AppsList local_and_server_apps = CreateAppsList("https://example.com/", 10);
  AppsList expected_apps_to_install =
      CreateAppsList("https://example.org/", 10);
  // These fields are not synced, these are just expected values.
  for (std::unique_ptr<WebApp>& expected_app_to_install :
       expected_apps_to_install) {
    expected_app_to_install->SetInstallState(
        AreAppsLocallyInstalledBySync()
            ? proto::InstallState::INSTALLED_WITH_OS_INTEGRATION
            : proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
    expected_app_to_install->SetIsFromSyncAndPendingInstallation(true);
  }

  Registry registry;
  InsertAppsListIntoRegistry(&registry, local_and_server_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();

  syncer::EntityChangeList sync_data_list;
  ConvertAppsListToEntityChangeList(expected_apps_to_install, &sync_data_list);
  ConvertAppsListToEntityChangeList(local_and_server_apps, &sync_data_list);

  EXPECT_CALL(processor(), Put).Times(0);

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

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_EmptyEntityChanges) {
  AppsList merged_apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();
  MergeFullSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_AddUpdateDelete) {
  // 20 initial apps with DisplayMode::kStandalone user display mode.
  AppsList merged_apps = CreateAppsList("https://example.com/", 20);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();
  MergeFullSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;

  for (std::unique_ptr<WebApp>& app_to_add :
       CreateAppsList("https://example.org/", 10)) {
    app_to_add->SetInstallState(
        AreAppsLocallyInstalledBySync()
            ? proto::InstallState::INSTALLED_WITH_OS_INTEGRATION
            : proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
    app_to_add->SetIsFromSyncAndPendingInstallation(true);

    ConvertAppToEntityChange(*app_to_add, syncer::EntityChange::ACTION_ADD,
                             &entity_changes);
  }

  // Update first 5 initial apps.
  for (int i = 0; i < 5; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*merged_apps[i]);
    // Update user display mode field.
    app_to_update->SetUserDisplayMode(UserDisplayMode::kBrowser);
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
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

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
          [&](const std::vector<webapps::AppId>& apps_to_uninstall,
              WebAppSyncBridge::RepeatingUninstallCallback callback) {
            EXPECT_EQ(5ul, apps_to_uninstall.size());
            for (const webapps::AppId& app_to_uninstall : apps_to_uninstall) {
              // The app must be registered.
              const WebApp* app = registrar().GetAppById(app_to_uninstall);
              // Sync expects that the apps are deleted by the delegate.
              EXPECT_TRUE(app);
              EXPECT_TRUE(app->is_uninstalling());
              EXPECT_TRUE(app->GetSources().empty());
              registry.erase(app_to_uninstall);
              {
                ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
                update->DeleteApp(app_to_uninstall);
              }
              callback.Run(app_to_uninstall,
                           webapps::UninstallResultCode::kAppRemoved);
            }

            barrier_closure.Run();
          }));

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_DeleteHappensExternally) {
  // 5 initial apps.
  AppsList merged_apps = CreateAppsList("https://example.com/", 5);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();
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
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  base::RunLoop run_loop;
  std::vector<webapps::AppId> to_uninstall;
  WebAppSyncBridge::RepeatingUninstallCallback uninstall_complete_callback;

  sync_bridge().SetUninstallFromSyncCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const std::vector<webapps::AppId>& apps_to_uninstall,
              WebAppSyncBridge::RepeatingUninstallCallback callback) {
            to_uninstall = apps_to_uninstall;
            uninstall_complete_callback = callback;
            run_loop.Quit();
          }));

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));
  run_loop.Run();

  EXPECT_EQ(5ul, to_uninstall.size());

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
  for (const webapps::AppId& app_to_uninstall : to_uninstall) {
    const WebApp* app = registrar().GetAppById(app_to_uninstall);
    EXPECT_TRUE(app);
    EXPECT_TRUE(app->is_uninstalling());
    EXPECT_TRUE(app->GetSources().empty());
  }
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_UpdateOnly) {
  AppsList merged_apps = CreateAppsList("https://example.com/", 10);

  Registry registry;
  InsertAppsListIntoRegistry(&registry, merged_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();
  MergeFullSyncData(merged_apps);

  syncer::EntityChangeList entity_changes;

  // Update last 5 initial apps.
  for (int i = 5; i < 10; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*merged_apps[i]);
    app_to_update->SetUserDisplayMode(UserDisplayMode::kStandalone);

    sync_pb::WebAppSpecifics sync_data = app_to_update->sync_proto();
    sync_data.set_name("Sync Name");
    sync_data.set_theme_color(SK_ColorYELLOW);
    app_to_update->SetSyncProto(std::move(sync_data));

    ConvertAppToEntityChange(
        *app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
    // Override the app in the expected registry.
    registry[app_to_update->app_id()] = std::move(app_to_update);
  }

  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  // No installs or uninstalls are made here, only app updates.
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
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
  StartWebAppProvider();

  syncer::EntityChangeList entity_changes;

  // Install 5 kSync apps over existing kPolicy apps. Leave the rest unchanged.
  for (int i = 0; i < 5; ++i) {
    const WebApp& app_to_install = *policy_apps[i];
    ConvertAppToEntityChange(app_to_install, syncer::EntityChange::ACTION_ADD,
                             &entity_changes);
  }

  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<WebApp>& expected_sync_and_policy_app =
        registry[policy_apps[i]->app_id()];
    expected_sync_and_policy_app->AddSource(WebAppManagement::kSync);
  }

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
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
  StartWebAppProvider();
  MergeFullSyncData(policy_and_sync_apps);

  syncer::EntityChangeList entity_changes;

  // Update first 5 kSync apps which are shared with kPolicy. Leave the rest
  // unchanged.
  AppsList apps_to_update;
  for (int i = 0; i < 5; ++i) {
    auto app_to_update = std::make_unique<WebApp>(*policy_and_sync_apps[i]);
    app_to_update->SetUserDisplayMode(UserDisplayMode::kBrowser);

    sync_pb::WebAppSpecifics sync_data = app_to_update->sync_proto();
    sync_data.set_name("Updated Sync Name");
    sync_data.set_theme_color(SK_ColorWHITE);
    app_to_update->SetSyncProto(std::move(sync_data));

    ConvertAppToEntityChange(
        *app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
    apps_to_update.push_back(std::move(app_to_update));
  }

  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i)
    registry[policy_and_sync_apps[i]->app_id()] = std::move(apps_to_update[i]);

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
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
  StartWebAppProvider();
  MergeFullSyncData(policy_and_sync_apps);

  syncer::EntityChangeList entity_changes;

  // Uninstall 5 kSync apps which are shared with kPolicy. Leave the rest
  // unchanged.
  for (int i = 0; i < 5; ++i) {
    const WebApp& app_to_uninstall = *policy_and_sync_apps[i];
    ConvertAppToEntityChange(
        app_to_uninstall, syncer::EntityChange::ACTION_DELETE, &entity_changes);
  }

  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);
  SetSyncInstallCallbackFailureIfCalled();

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  // Modify the registry with the results that we expect.
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<WebApp>& expected_policy_app =
        registry[policy_and_sync_apps[i]->app_id()];
    expected_policy_app->RemoveSource(WebAppManagement::kSync);
  }

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

// Commits local data (e.g. installed web apps) before sync is hooked up. This
// tests that the web apps are correctly sent to USS after MergeFullSyncData is
// called.
TEST_F(WebAppSyncBridgeTest, CommitUpdate_CommitWhileNotTrackingMetadata) {
  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
  StartWebAppProvider();

  AppsList sync_apps = CreateAppsList("https://example.com/", 10);
  Registry expected_registry;
  InsertAppsListIntoRegistry(&expected_registry, sync_apps);

  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);
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
  ON_CALL(processor(), Put)
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_TRUE(RemoveEntityDataAppFromAppsList(storage_key, *entity_data,
                                                    &sync_apps));
        if (sync_apps.empty())
          run_loop.Quit();
      });

  EXPECT_CALL(processor(), Delete).Times(0);
  EXPECT_CALL(processor(), IsTrackingMetadata())
      .WillOnce(testing::Return(true));

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  syncer::EntityChangeList{});
  run_loop.Run();

  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), expected_registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, CommitUpdate_CreateSyncApp) {
  StartWebAppProvider();

  AppsList sync_apps = CreateAppsList("https://example.com/", 10);
  Registry expected_registry;
  InsertAppsListIntoRegistry(&expected_registry, sync_apps);

  ON_CALL(processor(), Put)
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        ASSERT_TRUE(base::Contains(expected_registry, storage_key));
        const std::unique_ptr<WebApp>& expected_app =
            expected_registry.at(storage_key);
        EXPECT_TRUE(IsSyncDataEqual(*expected_app, *entity_data));
        RemoveWebAppFromAppsList(&sync_apps, storage_key);
      });
  EXPECT_CALL(processor(), Delete).Times(0);
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
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), expected_registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, CommitUpdate_UpdateSyncApp) {
  AppsList sync_apps = CreateAppsList("https://example.com/", 10);
  Registry registry;
  InsertAppsListIntoRegistry(&registry, sync_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();

  ON_CALL(processor(), Put)
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        ASSERT_TRUE(base::Contains(registry, storage_key));
        const std::unique_ptr<WebApp>& expected_app = registry.at(storage_key);
        EXPECT_TRUE(IsSyncDataEqual(*expected_app, *entity_data));
        RemoveWebAppFromAppsList(&sync_apps, storage_key);
      });
  EXPECT_CALL(processor(), Delete).Times(0);

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());

    for (const std::unique_ptr<WebApp>& app : sync_apps) {
      // Obtain a writeable handle.
      WebApp* sync_app = update->UpdateApp(app->app_id());

      sync_pb::WebAppSpecifics sync_data;
      sync_data.set_name("Updated Sync Name");
      sync_data.set_theme_color(SK_ColorBLACK);
      sync_app->SetSyncProto(std::move(sync_data));
      sync_app->SetUserDisplayMode(UserDisplayMode::kBrowser);

      // Override the app in the expected registry.
      registry[sync_app->app_id()] = std::make_unique<WebApp>(*sync_app);
    }
  }
  EXPECT_TRUE(future.Take());

  EXPECT_TRUE(sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, CommitUpdate_DeleteSyncApp) {
  const int kNumApps = 5;
  AppsList sync_apps = CreateAppsList("https://example.com/", kNumApps);
  Registry registry;
  InsertAppsListIntoRegistry(&registry, sync_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();

  // Put() is called kNumApps times, since UninstallWebApp() calls Put() to
  // update the `is_uninstalling` field.
  EXPECT_CALL(processor(), Put).Times(kNumApps);
  ON_CALL(processor(), Delete)
      .WillByDefault([&](const std::string& storage_key,
                         const syncer::DeletionOrigin& origin,
                         syncer::MetadataChangeList* metadata) {
        EXPECT_TRUE(base::Contains(registry, storage_key));
        RemoveWebAppFromAppsList(&sync_apps, storage_key);
        // Delete the app in the expected registry.
        registry.erase(storage_key);
      });

  base::test::TestFuture<const std::optional<std::string>&> final_callback;
  fake_provider().scheduler().UninstallAllUserInstalledWebApps(
      webapps::WebappUninstallSource::kSync, final_callback.GetCallback());
  EXPECT_TRUE(final_callback.Wait());

  EXPECT_TRUE(sync_apps.empty());
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
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
  StartWebAppProvider();

  ON_CALL(processor(), Put)
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
        entity_data_app->SetInstallState(
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);

        EXPECT_TRUE(IsSyncDataEqualIfApplied(
            expected_app, std::move(entity_data_app), *entity_data));

        RemoveWebAppFromAppsList(&policy_apps, storage_key);
      });
  EXPECT_CALL(processor(), Delete).Times(0);

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
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
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
  StartWebAppProvider();

  ON_CALL(processor(), Put)
      .WillByDefault([&](const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity_data,
                         syncer::MetadataChangeList* metadata) {
        // Local changes to synced apps cause excessive |Put|.
        // See TODO in WebAppSyncBridge::UpdateSync.
        RemoveWebAppFromAppsList(&policy_and_sync_apps, storage_key);
      });
  ON_CALL(processor(), Delete)
      .WillByDefault([&](const std::string& storage_key,
                         const syncer::DeletionOrigin& origin,
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
  EXPECT_TRUE(IsRegistryEqual(registrar_registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

// Test that any apps that are still pending install from sync (or,
// |is_from_sync_and_pending_installation|) are continued to be installed when
// the bridge initializes.
TEST_F(WebAppSyncBridgeTest, InstallAppsFromSyncAndPendingInstallation) {
  AppsList apps_in_sync_install = CreateAppsList("https://example.com/", 10);
  for (std::unique_ptr<WebApp>& app : apps_in_sync_install) {
    app->SetInstallState(
        AreAppsLocallyInstalledBySync()
            ? proto::InstallState::INSTALLED_WITH_OS_INTEGRATION
            : proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
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

  StartWebAppProvider();

  run_loop.Run();
}

// Tests that non user installable apps can also be removed by the
// WebAppSyncBridge during system startup, if `is_uninstalling` is set to true.
// Test for crbug.com/335253048, by using kSystem to mock that behavior. Since
// System Web Apps are only on Ash chrome, kPolicy is used instead on Lacro
TEST_F(WebAppSyncBridgeTest, CanDeleteNonUserInstallableApps) {
  AppsList system_apps;

  // This app should be uninstalled, since the `is_uninstalling` field is set.
  std::unique_ptr<WebApp> app1 =
      test::CreateWebApp(GURL("https://example.com/app1"));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  app1->AddSource(WebAppManagement::kPolicy);
#else
  app1->AddSource(WebAppManagement::kSystem);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  app1->SetIsUninstalling(/*is_uninstalling=*/true);
  const webapps::AppId app_id1 = app1->app_id();
  system_apps.push_back(std::move(app1));

  // This app will not be uninstalled.
  std::unique_ptr<WebApp> app2 =
      test::CreateWebApp(GURL("https://example.com/app2"));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  app2->AddSource(WebAppManagement::kPolicy);
#else
  app2->AddSource(WebAppManagement::kSystem);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  const webapps::AppId app_id2 = app2->app_id();
  system_apps.push_back(std::move(app2));

  Registry registry;
  InsertAppsListIntoRegistry(&registry, system_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();

  EXPECT_FALSE(registrar().IsInstalled(app_id1));
  EXPECT_TRUE(registrar().IsInstalled(app_id2));
}

// Tests that OnWebAppsWillBeUpdatedFromSync observer notification is called
// properly.
TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_OnWebAppsWillBeUpdatedFromSync) {
  AppsList initial_registry_apps = CreateAppsList("https://example.com/", 10);
  for (std::unique_ptr<WebApp>& app : initial_registry_apps)
    app->SetUserDisplayMode(UserDisplayMode::kBrowser);
  StartWebAppProviderFromAppList(initial_registry_apps);

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
                    UserDisplayMode::kBrowser);
          EXPECT_EQ(new_app_state->user_display_mode(),
                    UserDisplayMode::kStandalone);

          // new and old states must be equal if diff fixed:
          auto old_app_state_no_diff = std::make_unique<WebApp>(*old_app_state);
          old_app_state_no_diff->SetUserDisplayMode(
              UserDisplayMode::kStandalone);
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
    app_server_state->SetUserDisplayMode(UserDisplayMode::kStandalone);
    apps_server_state.push_back(std::move(app_server_state));
  }

  sync_bridge_test_utils::UpdateApps(sync_bridge(), apps_server_state);

  run_loop.Run();

  // 5 other apps left unchanged:
  EXPECT_EQ(5u, initial_registry_apps.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(UserDisplayMode::kBrowser,
              initial_registry_apps[i]->user_display_mode());
  }
}

TEST_F(WebAppSyncBridgeTest, RetryIncompleteUninstalls) {
  AppsList initial_registry_apps = CreateAppsList("https://example.com/", 5);
  std::vector<webapps::AppId> initial_app_ids;
  for (std::unique_ptr<WebApp>& app : initial_registry_apps) {
    app->SetUserDisplayMode(UserDisplayMode::kBrowser);
    app->SetIsUninstalling(true);
    initial_app_ids.push_back(app->app_id());
  }

  SetSyncInstallCallbackFailureIfCalled();

  base::RunLoop run_loop;
  sync_bridge().SetRetryIncompleteUninstallsCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const base::flat_set<webapps::AppId>& apps_to_uninstall) {
            EXPECT_EQ(apps_to_uninstall.size(), 5ul);
            EXPECT_THAT(apps_to_uninstall, ::testing::UnorderedElementsAreArray(
                                               apps_to_uninstall));
            run_loop.Quit();
          }));

  StartWebAppProviderFromAppList(initial_registry_apps);

  run_loop.Run();
}

syncer::EntityData CreateSyncEntityData(
    const std::string& name,
    const sync_pb::EntitySpecifics& specifics) {
  syncer::EntityData entity_data;
  entity_data.name = name;
  entity_data.specifics = specifics;
  return entity_data;
}

TEST_F(WebAppSyncBridgeTest, InvalidSyncData) {
  StartWebAppProvider();

  base::HistogramTester histogram_tester;

  GURL url("https://example.com/start");
  const std::string app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, url);
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_web_app()->set_name(app_id);

  // No start url.
  EXPECT_FALSE(sync_bridge().IsEntityDataValid(
      CreateSyncEntityData(app_id, entity_specifics)));

  // Invalid start url.
  entity_specifics.mutable_web_app()->set_start_url("");
  EXPECT_FALSE(sync_bridge().IsEntityDataValid(
      CreateSyncEntityData(app_id, entity_specifics)));

  // Invalid manifest id.
  entity_specifics.mutable_web_app()->set_start_url("about:blank");
  entity_specifics.mutable_web_app()->set_relative_manifest_id("");
  EXPECT_FALSE(sync_bridge().IsEntityDataValid(
      CreateSyncEntityData(app_id, entity_specifics)));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Sync.InvalidEntity"),
              base::BucketsAre(
                  base::Bucket(StorageKeyParseResult::kNoStartUrl, 1),
                  base::Bucket(StorageKeyParseResult::kInvalidStartUrl, 1),
                  base::Bucket(StorageKeyParseResult::kInvalidManifestId, 1)));
}

// Test that a serialized proto with an unrecognized new field can successfully
// sync install in the current version and preserves the field value.
TEST_F(WebAppSyncBridgeTest, SpecificsProtoWithNewFieldPreserved) {
  // Serialized in M125 by modifying web_app_specifics.proto:
  // +optional string test_new_field = 5372767;
  // Then:
  // const char kAppName[] = "Test name";
  // sync_pb::WebAppSpecifics sync_proto;
  // sync_proto.set_start_url(kStartUrl);
  // sync_proto.set_name(kAppName);
  // sync_proto.set_user_display_mode_default(
  //     sync_pb::WebAppSpecifics::BROWSER);
  // sync_proto.set_test_new_field("hello");
  // sync_proto.SerializeAsString();
  const char kStartUrl[] = "https://example.com/launchurl";
  const std::string serialized_proto = {
      10,  29,  104, 116, 116, 112, 115, 58,  47,  47,  101, 120, 97,  109,
      112, 108, 101, 46,  99,  111, 109, 47,  108, 97,  117, 110, 99,  104,
      117, 114, 108, 18,  9,   84,  101, 115, 116, 32,  110, 97,  109, 101,
      24,  1,   -6,  -75, -65, 20,  5,   104, 101, 108, 108, 111};
  const GURL start_url = GURL(kStartUrl);
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);

  // Parse the proto.
  sync_pb::WebAppSpecifics sync_proto;
  bool parsed = sync_proto.ParseFromString(serialized_proto);

  // Sanity check the proto was parsed.
  ASSERT_TRUE(parsed);
  EXPECT_EQ(kStartUrl, sync_proto.start_url());

  StartWebAppProvider();

  // Listen for sync installs.
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  EXPECT_FALSE(sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(),
      ToEntityChangeList(app_id, sync_proto, sync_bridge())));

  // Await sync install.
  EXPECT_EQ(install_observer.Wait(), app_id);

  const WebApp* app = fake_provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);

  // Clear the fields added due to normalizing the proto in `SetSyncProto` and
  // `ApplySyncDataToApp`.
  sync_pb::WebAppSpecifics result_proto = app->sync_proto();
  result_proto.clear_relative_manifest_id();
#if BUILDFLAG(IS_CHROMEOS)
  result_proto.clear_user_display_mode_cros();
#endif

  // Check that the sync proto retained its value, including the unknown field.
  EXPECT_EQ(result_proto.SerializeAsString(), serialized_proto);
}

TEST_F(WebAppSyncBridgeTest, MigratePartiallyInstalledToCorrectStatus) {
  AppsList initial_registry_apps = CreateAppsList("https://example.com/", 10);
  for (auto& app : initial_registry_apps) {
    app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  }
  Registry registry;
  InsertAppsListIntoRegistry(&registry, initial_registry_apps);
  database_factory().WriteRegistry(registry);
  StartWebAppProvider();

  for (const webapps::AppId& app_id : registrar().GetAppIds()) {
    EXPECT_EQ(registrar().GetAppById(app_id)->install_state(),
              proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  }
}

namespace {
using UserDisplayModeSplitParam = std::tuple<
    std::optional<WebAppSpecifics_UserDisplayMode> /*sync_cros_udm*/,
    std::optional<WebAppSpecifics_UserDisplayMode> /*sync_non_cros_udm*/,
    std::optional<UserDisplayMode> /*installed_udm*/,
    std::optional<
        WebAppSpecifics_UserDisplayMode> /*local_other_platform_udm*/>;

constexpr std::optional<WebAppSpecifics_UserDisplayMode>
    kSyncUserDisplayModes[]{std::nullopt,
                            WebAppSpecifics_UserDisplayMode_UNSPECIFIED,
                            WebAppSpecifics_UserDisplayMode_BROWSER,
                            WebAppSpecifics_UserDisplayMode_STANDALONE};

constexpr std::optional<UserDisplayMode> kInstalledUserDisplayModes[]{
    std::nullopt, UserDisplayMode::kBrowser, UserDisplayMode::kStandalone};

std::string ToString(std::optional<WebAppSpecifics_UserDisplayMode> udm) {
  if (!udm.has_value()) {
    return "absent";
  }
  switch (udm.value()) {
    case WebAppSpecifics_UserDisplayMode_UNSPECIFIED:
      return "unspecified";
    case WebAppSpecifics_UserDisplayMode_BROWSER:
      return "browser";
    case WebAppSpecifics_UserDisplayMode_STANDALONE:
      return "standalone";
    case WebAppSpecifics_UserDisplayMode_TABBED:
      NOTREACHED();
  }
}

std::string ToString(std::optional<UserDisplayMode> udm) {
  if (!udm.has_value()) {
    return "absent";
  }
  switch (udm.value()) {
    case UserDisplayMode::kBrowser:
      return "browser";
    case UserDisplayMode::kStandalone:
      return "standalone";
    case UserDisplayMode::kTabbed:
      NOTREACHED();
  }
}
}  // namespace

class WebAppSyncBridgeTest_UserDisplayModeSplit
    : public WebAppTest,
      public testing::WithParamInterface<UserDisplayModeSplitParam> {
 public:
  static std::string ParamToString(
      testing::TestParamInfo<UserDisplayModeSplitParam> param) {
    return base::StrCat({
        "SyncCrosUdm_",
        ToString(std::get<0>(param.param)),
        "_SyncNonCrosUdm_",
        ToString(std::get<1>(param.param)),
        "_InstalledUdm_",
        ToString(std::get<2>(param.param)),
        "_OtherPlatformUdm_",
        ToString(std::get<3>(param.param)),
    });
  }

  WebAppSyncBridgeTest_UserDisplayModeSplit() {
#if BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.InitWithFeatureStates({
        // UDM mitigations mess with the installed local state, disable them so
        // the state matches the intention of the test.
        {kUserDisplayModeSyncBrowserMitigation, false},
        {kUserDisplayModeSyncStandaloneMitigation, false},
    });
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  ~WebAppSyncBridgeTest_UserDisplayModeSplit() override = default;

  std::optional<WebAppSpecifics_UserDisplayMode> sync_cros_udm() const {
    return std::get<0>(GetParam());
  }
  std::optional<WebAppSpecifics_UserDisplayMode> sync_non_cros_udm() const {
    return std::get<1>(GetParam());
  }
  // UDM for the current platform. Absent means it's not locally installed.
  std::optional<UserDisplayMode> installed_udm() const {
    return std::get<2>(GetParam());
  }
  // UDM stored locally for the other platform.
  std::optional<WebAppSpecifics_UserDisplayMode> local_other_platform_udm()
      const {
    return std::get<3>(GetParam());
  }

  bool IsChromeOs() const {
#if BUILDFLAG(IS_CHROMEOS)
    return true;
#else
    return false;
#endif
  }

  std::optional<WebAppSpecifics_UserDisplayMode> sync_current_platform_udm()
      const {
    return IsChromeOs() ? sync_cros_udm() : sync_non_cros_udm();
  }
  std::optional<WebAppSpecifics_UserDisplayMode> sync_other_platform_udm()
      const {
    return IsChromeOs() ? sync_non_cros_udm() : sync_cros_udm();
  }

  bool installed_before_sync() const { return installed_udm().has_value(); }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
  WebAppSyncBridge& sync_bridge() { return provider().sync_bridge_unsafe(); }

  UserDisplayMode ToMojomUdmFallbackToStandalone(
      std::optional<WebAppSpecifics_UserDisplayMode> sync_udm) {
    if (!sync_udm.has_value()) {
      return mojom::UserDisplayMode::kStandalone;
    }
    return ToMojomUserDisplayMode(sync_udm.value());
  }

 protected:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppSyncBridgeTest_UserDisplayModeSplit, SyncUpdateToUserDisplayMode) {
  GURL start_url = GURL("https://example.com/app");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);

  // Install an app.
  if (installed_before_sync()) {
    auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    info->scope = start_url;
    info->title = u"Basic web app";
    info->description = u"Test description";
    info->user_display_mode = installed_udm().value();

    webapps::AppId installed_app_id =
        test::InstallWebApp(profile(), std::move(info));
    DCHECK_EQ(installed_app_id, app_id);

    // Set the local value of the other platform's UDM.
    if (local_other_platform_udm()) {
      ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
      WebApp* web_app = update->UpdateApp(app_id);
      DCHECK(web_app);
      sync_pb::WebAppSpecifics sync_proto = web_app->sync_proto();

      if (IsChromeOs()) {
        sync_proto.set_user_display_mode_default(
            local_other_platform_udm().value());
      } else {
        sync_proto.set_user_display_mode_cros(
            local_other_platform_udm().value());
      }
      web_app->SetSyncProto(std::move(sync_proto));
    }
  }

  // Listen for sync installs.
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  // Update web app in sync profile.
  sync_pb::WebAppSpecifics sync_proto;
  sync_proto.set_start_url(start_url.spec());
  sync_proto.set_scope(start_url.spec());
  sync_proto.set_relative_manifest_id("app");
  sync_proto.set_name("Basic web app");
  if (sync_cros_udm()) {
    sync_proto.set_user_display_mode_cros(sync_cros_udm().value());
  }
  if (sync_non_cros_udm()) {
    sync_proto.set_user_display_mode_default(sync_non_cros_udm().value());
  }

  EXPECT_FALSE(sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(),
      ToEntityChangeList(app_id, sync_proto, sync_bridge())));

  // Await sync install.
  if (!installed_before_sync()) {
    EXPECT_EQ(install_observer.Wait(), app_id);
  }

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);

  if (sync_current_platform_udm()) {
    // If UDM is set for the current platform in sync, it should be used.
    EXPECT_EQ(app->user_display_mode(),
              ToMojomUdmFallbackToStandalone(sync_current_platform_udm()));
  } else if (installed_before_sync()) {
    // Otherwise we should preserve a local UDM value if available.
    EXPECT_EQ(app->user_display_mode(), installed_udm());
  } else {
    if (IsChromeOs()) {
      // CrOS should populate a UDM value from non-CrOS, falling back to
      // standalone.
      EXPECT_EQ(app->user_display_mode(),
                ToMojomUdmFallbackToStandalone(sync_other_platform_udm()));
    } else {
      // Non-CrOS should fall back to standalone.
      EXPECT_EQ(app->user_display_mode(), UserDisplayMode::kStandalone);
    }
  }

  std::optional<sync_pb::WebAppSpecifics::UserDisplayMode>
      app_this_platform_udm;
  std::optional<sync_pb::WebAppSpecifics::UserDisplayMode>
      app_other_platform_udm;
  if (IsChromeOs()) {
    if (app->sync_proto().has_user_display_mode_cros()) {
      app_this_platform_udm = app->sync_proto().user_display_mode_cros();
    }
    if (app->sync_proto().has_user_display_mode_default()) {
      app_other_platform_udm = app->sync_proto().user_display_mode_default();
    }
  } else {
    if (app->sync_proto().has_user_display_mode_default()) {
      app_this_platform_udm = app->sync_proto().user_display_mode_default();
    }
    if (app->sync_proto().has_user_display_mode_cros()) {
      app_other_platform_udm = app->sync_proto().user_display_mode_cros();
    }
  }
  EXPECT_EQ(app->user_display_mode(),
            ToMojomUdmFallbackToStandalone(app_this_platform_udm));

  if (sync_other_platform_udm()) {
    // If UDM is set for the other platform in sync, it should be preserved
    // (though Unspecified values currently become standalone).
    EXPECT_EQ(app_other_platform_udm, sync_other_platform_udm());
  } else if (installed_before_sync()) {
    // Otherwise, if installed, we should preserve a local UDM value (or unset).
    EXPECT_EQ(app_other_platform_udm, local_other_platform_udm());
  } else {
    // Otherwise, it should remain unset.
    EXPECT_EQ(app_other_platform_udm, std::nullopt);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    WebAppSyncBridgeTest_UserDisplayModeSplit,
    testing::Combine(
        /*sync_cros_udm=*/testing::ValuesIn(kSyncUserDisplayModes),
        /*sync_non_cros_udm=*/testing::ValuesIn(kSyncUserDisplayModes),
        /*installed_udm=*/testing::ValuesIn(kInstalledUserDisplayModes),
        /*local_other_platform_udm=*/
        testing::ValuesIn(kSyncUserDisplayModes)),
    WebAppSyncBridgeTest_UserDisplayModeSplit::ParamToString);

}  // namespace web_app
