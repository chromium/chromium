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
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/sync/base/deletion_origin.h"
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
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

using testing::_;

using AppsList = std::vector<std::unique_ptr<WebApp>>;

using mojom::UserDisplayMode;
using sync_pb::WebAppSpecifics;
using sync_pb::WebAppSpecifics_UserDisplayMode;
using sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER;
using sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE;
using sync_pb::WebAppSpecifics_UserDisplayMode_TABBED;
using sync_pb::WebAppSpecifics_UserDisplayMode_UNSPECIFIED;

// Creates a protobuf web app that passes the parsing checks.
proto::WebApp CreateWebAppProtoForTesting(const std::string& name,
                                          const GURL& start_url) {
  proto::WebApp web_app;
  CHECK(start_url.is_valid());
  web_app.set_name(name);
  web_app.mutable_sync_data()->set_name(name);
  web_app.mutable_sync_data()->set_start_url(start_url.spec());
  webapps::ManifestId manifest_id =
      GenerateManifestIdFromStartUrlOnly(start_url);
  web_app.mutable_sync_data()->set_relative_manifest_id(
      RelativeManifestIdPath(manifest_id));
  web_app.mutable_sync_data()->set_user_display_mode_default(
      WebAppSpecifics_UserDisplayMode::
          WebAppSpecifics_UserDisplayMode_STANDALONE);
  web_app.set_scope(start_url.GetWithoutFilename().spec());
  web_app.mutable_sources()->set_user_installed(true);
#if BUILDFLAG(IS_CHROMEOS)
  web_app.mutable_chromeos_data();
  web_app.mutable_sync_data()->set_user_display_mode_cros(
      WebAppSpecifics_UserDisplayMode::
          WebAppSpecifics_UserDisplayMode_STANDALONE);
#endif
  web_app.set_install_state(
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  return web_app;
}

webapps::AppId GetAppIdFromWebAppProto(const proto::WebApp& web_app) {
  CHECK(web_app.has_sync_data());
  CHECK(web_app.sync_data().has_relative_manifest_id());
  CHECK(web_app.sync_data().has_start_url());
  GURL start_url = GURL(web_app.sync_data().start_url());
  CHECK(start_url.is_valid());
  return GenerateAppId(web_app.sync_data().relative_manifest_id(), start_url);
}

webapps::AppId GetAppIdFromWebAppSpecifics(const WebAppSpecifics& specifics) {
  CHECK(specifics.has_relative_manifest_id());
  CHECK(specifics.has_start_url());
  GURL start_url = GURL(specifics.start_url());
  CHECK(start_url.is_valid());
  return GenerateAppId(specifics.relative_manifest_id(), start_url);
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
      entity_change = syncer::EntityChange::CreateDelete(app.app_id(),
                                                         syncer::EntityData());
      break;
  }

  sync_data_list->push_back(std::move(entity_change));
}

syncer::EntityChangeList ToEntityChangeList(const webapps::AppId& app_id,
                                            const WebAppSpecifics& sync_proto,
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

// Creates the minimal WebAppSpecifics.
WebAppSpecifics CreateWebAppSpecificsForTesting(const std::string& name,
                                                const GURL& start_url) {
  WebAppSpecifics web_app_specifics;
  CHECK(start_url.is_valid());
  web_app_specifics.set_name(name);
  web_app_specifics.set_start_url(start_url.spec());
  webapps::ManifestId manifest_id =
      GenerateManifestIdFromStartUrlOnly(start_url);
  web_app_specifics.set_relative_manifest_id(
      RelativeManifestIdPath(manifest_id));
  SetPlatformSpecificUserDisplayMode(
      WebAppSpecifics_UserDisplayMode::
          WebAppSpecifics_UserDisplayMode_STANDALONE,
      &web_app_specifics);
  web_app_specifics.set_scope(start_url.GetWithoutFilename().spec());
  return web_app_specifics;
}

std::vector<WebAppSpecifics> CreateSyncWebApps(const std::string& base_url,
                                               int num_apps) {
  std::vector<WebAppSpecifics> apps_list;

  for (int i = 0; i < num_apps; ++i) {
    std::string name = base::StrCat({"App at ", base_url});
    GURL start_url = GURL(base::StrCat({base_url, base::NumberToString(i)}));
    apps_list.push_back(CreateWebAppSpecificsForTesting(name, start_url));
  }
  return apps_list;
}

void ConvertSpecificsToEntityChange(
    const WebAppSpecifics& app,
    syncer::EntityChange::ChangeType change_type,
    syncer::EntityChangeList* sync_data_list) {
  std::unique_ptr<syncer::EntityChange> entity_change;
  syncer::EntityData entity_data;
  entity_data.name = app.name();
  *(entity_data.specifics.mutable_web_app()) = app;

  webapps::AppId app_id = GetAppIdFromWebAppSpecifics(app);

  switch (change_type) {
    case syncer::EntityChange::ACTION_ADD:
      entity_change =
          syncer::EntityChange::CreateAdd(app_id, std::move(entity_data));
      break;
    case syncer::EntityChange::ACTION_UPDATE:
      entity_change =
          syncer::EntityChange::CreateUpdate(app_id, std::move(entity_data));
      break;
    case syncer::EntityChange::ACTION_DELETE:
      entity_change =
          syncer::EntityChange::CreateDelete(app_id, syncer::EntityData());
      break;
  }

  sync_data_list->push_back(std::move(entity_change));
}

void ConvertSpecificsListToEntityChangeList(
    const std::vector<WebAppSpecifics>& apps_list,
    syncer::EntityChangeList* sync_data_list) {
  for (const WebAppSpecifics& app : apps_list) {
    ConvertSpecificsToEntityChange(app, syncer::EntityChange::ACTION_ADD,
                                   sync_data_list);
  }
}

class WebAppSyncBridgeTest : public WebAppTest {
 public:
  void StartWebAppProvider() {
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void MergeSyncDataNoLocalChanges(
      const std::vector<WebAppSpecifics>& merged_apps) {
    syncer::EntityChangeList entity_data_list;
    ConvertSpecificsListToEntityChangeList(merged_apps, &entity_data_list);
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

  std::vector<webapps::AppId> InstallDummyApps(
      const std::string& base_url,
      int num_apps,
      webapps::WebappInstallSource install_source =
          webapps::WebappInstallSource::SYNC) {
    std::vector<webapps::AppId> apps_list;

    for (int i = 0; i < num_apps; ++i) {
      apps_list.push_back(test::InstallDummyWebApp(
          profile(),
          /*app_name=*/base::StrCat({"App at ", base_url}), /*app_url=*/
          GURL(base::StrCat({base_url, base::NumberToString(i)})),
          install_source));
    }
    return apps_list;
  }

  WebAppSpecifics GetSpecificsFromInstalledApp(const webapps::AppId& app_id) {
    return CreateSyncEntityData(*registrar().GetAppById(app_id))
        ->specifics.web_app();
  }
  std::vector<WebAppSpecifics> GetSpecificsFromInstalledApps(
      const std::vector<webapps::AppId> app_ids) {
    std::vector<WebAppSpecifics> specifics;
    for (const webapps::AppId& app_id : app_ids) {
      specifics.push_back(CreateSyncEntityData(*registrar().GetAppById(app_id))
                              ->specifics.web_app());
    }
    return specifics;
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

TEST_F(WebAppSyncBridgeTest, SyncEntityMatchesApp) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  // Install from non-sync source, so initially not visible to sync.
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://www.example.com/index.html"),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  WebAppSyncBridge::StorageKeyList storage_keys;
  // Add an unknown key to test this is handled gracefully.
  storage_keys.push_back("unknown");
  storage_keys.push_back(app_id);

  // First, verify that it doesn't exist if kSync isn't a source.
  std::unique_ptr<syncer::DataBatch> data_batch =
      provider().sync_bridge_unsafe().GetDataForCommit(storage_keys);
  EXPECT_FALSE(data_batch->HasNext());

  // Add the new source, which enables output of the sync specifics.
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_id)->AddSource(WebAppManagement::kSync);
  }

  // Now, it should be populated.
  data_batch = provider().sync_bridge_unsafe().GetDataForCommit(storage_keys);
  ASSERT_TRUE(data_batch->HasNext());

  syncer::KeyAndData key_and_data = data_batch->Next();
  EXPECT_FALSE(data_batch->HasNext());
  ASSERT_NE("unknown", key_and_data.first);
  EXPECT_EQ(app_id, key_and_data.first);

  ASSERT_TRUE(key_and_data.second->specifics.has_web_app());
  const WebAppSpecifics& specifics = key_and_data.second->specifics.web_app();

  WebAppSpecifics expected;
  expected.set_name("Test App");
  expected.set_start_url("https://www.example.com/index.html");
  SetPlatformSpecificUserDisplayMode(WebAppSpecifics_UserDisplayMode_STANDALONE,
                                     &expected);
  expected.set_relative_manifest_id("index.html");
  expected.set_scope("https://www.example.com/");
  EXPECT_EQ(expected.SerializeAsString(), specifics.SerializeAsString());
}

TEST_F(WebAppSyncBridgeTest, PutCalled) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const webapps::AppId kAppId = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(kStartUrl));

  test::AwaitStartWebAppProviderAndSubsystems(profile());

  base::test::TestFuture<const std::string&,
                         std::unique_ptr<syncer::EntityData>,
                         syncer::MetadataChangeList*>
      put_future;

  EXPECT_CALL(processor(), Put).WillOnce(base::test::InvokeFuture(put_future));

  // Install from non-sync source, so initially not visible to sync.
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://www.example.com/index.html"),
      webapps::WebappInstallSource::SYNC);
  EXPECT_EQ(app_id, kAppId);

  ASSERT_TRUE(put_future.Wait());
  EXPECT_EQ(put_future.Get<0>(), kAppId);

  WebAppSpecifics expected;
  expected.set_name("Test App");
  expected.set_start_url("https://www.example.com/index.html");
  SetPlatformSpecificUserDisplayMode(WebAppSpecifics_UserDisplayMode_STANDALONE,
                                     &expected);
  expected.set_relative_manifest_id("index.html");
  expected.set_scope("https://www.example.com/");

  const WebAppSpecifics& specifics =
      put_future.Get<std::unique_ptr<syncer::EntityData>>()
          ->specifics.web_app();
  EXPECT_EQ(expected.SerializeAsString(), specifics.SerializeAsString());
}

// Tests that the client & storage tags are correct for entity data.
TEST_F(WebAppSyncBridgeTest, Identities) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://www.example.com/index.html"));

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateSyncEntityData(*registrar().GetAppById(app_id));

  EXPECT_EQ(app_id, sync_bridge().GetClientTag(*entity_data));
  EXPECT_EQ(app_id, sync_bridge().GetStorageKey(*entity_data));
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
  StartWebAppProvider();

  // The incoming list of apps from the sync server.
  syncer::EntityChangeList sync_data_list;
  std::vector<WebAppSpecifics> sync_apps =
      CreateSyncWebApps("https://www.example.com", 10);
  ConvertSpecificsListToEntityChangeList(sync_apps, &sync_data_list);

  // The local app state is the same as the server state, so no changes should
  // be sent.
  EXPECT_CALL(processor(), Put).Times(0);

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));

  EXPECT_EQ(sync_apps.size(), registrar_registry().size());
  for (const WebAppSpecifics& sync_app : sync_apps) {
    webapps::AppId app_id = GetAppIdFromWebAppSpecifics(sync_app);
    const WebApp* app = registrar().GetAppById(app_id);
    ASSERT_TRUE(app);
    EXPECT_EQ(sync_app.SerializeAsString(),
              app->sync_proto().SerializeAsString());
    // Make sure other members were propagated through.
    EXPECT_EQ(sync_app.name(), app->untranslated_name());
    EXPECT_EQ(sync_app.name(), registrar().GetAppShortName(app_id));
  }
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetGreaterThanServerSet) {
  StartWebAppProvider();
  std::vector<webapps::AppId> local_and_server_apps =
      InstallDummyApps("https://www.localAndServer.com/", 10);
  std::vector<webapps::AppId> apps_to_upload =
      InstallDummyApps("https://www.localToUpload.com/", 10);

  auto metadata_change_list = sync_bridge().CreateMetadataChangeList();
  syncer::MetadataChangeList* metadata_ptr = metadata_change_list.get();

  // Create a sync entity change list to add only the `local_and_server_apps`
  // apps, with the same config.
  std::vector<WebAppSpecifics> local_app_sync_specifics;
  for (const auto& app_id : local_and_server_apps) {
    local_app_sync_specifics.push_back(
        registrar().GetAppById(app_id)->sync_proto());
  }
  syncer::EntityChangeList sync_data_list;
  ConvertSpecificsListToEntityChangeList(local_app_sync_specifics,
                                         &sync_data_list);

  // MergeFullSyncData below should send |expected_local_apps_to_upload| to the
  // processor() to upload to USS.
  base::flat_set<webapps::AppId> remaining_apps_for_server{apps_to_upload};
  base::RunLoop run_loop;
  EXPECT_CALL(processor(), Put)
      .Times(10)
      .WillRepeatedly([&](const std::string& storage_key,
                          std::unique_ptr<syncer::EntityData> entity_data,
                          syncer::MetadataChangeList* metadata) {
        EXPECT_EQ(metadata_ptr, metadata);
        remaining_apps_for_server.erase(storage_key);
        if (remaining_apps_for_server.empty()) {
          run_loop.Quit();
        }
      });

  sync_bridge().MergeFullSyncData(std::move(metadata_change_list),
                                  std::move(sync_data_list));
  run_loop.Run();

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, MergeFullSyncData_LocalSetLessThanServerSet) {
  StartWebAppProvider();

  EXPECT_CALL(processor(), Put).Times(1);
  std::vector<webapps::AppId> local_and_server_apps =
      InstallDummyApps("https://www.localAndServer.com/", 1);

  std::vector<WebAppSpecifics> sync_apps_to_install =
      CreateSyncWebApps("https://www.serverToLocal.com", 1);

  syncer::EntityChangeList sync_data_list;
  ConvertSpecificsListToEntityChangeList(sync_apps_to_install, &sync_data_list);
  ConvertSpecificsListToEntityChangeList(
      GetSpecificsFromInstalledApps(local_and_server_apps), &sync_data_list);

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));

  // Make sure all of the installs complete.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Verify the new install state.
  for (const WebAppSpecifics& sync_app : sync_apps_to_install) {
    const WebApp* web_app =
        registrar().GetAppById(GetAppIdFromWebAppSpecifics(sync_app));
    ASSERT_TRUE(web_app);
    EXPECT_EQ(web_app->sync_proto().SerializeAsString(),
              sync_app.SerializeAsString());
    proto::InstallState expected_install_state =
        AreAppsLocallyInstalledBySync()
            ? proto::InstallState::INSTALLED_WITH_OS_INTEGRATION
            : proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
    EXPECT_EQ(web_app->install_state(), expected_install_state);
    EXPECT_FALSE(web_app->is_from_sync_and_pending_installation());
  }

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_EmptyEntityChanges) {
  StartWebAppProvider();

  std::vector<webapps::AppId> local_and_server_apps =
      InstallDummyApps("https://www.localAndServer.com/", 1);
  MergeSyncDataNoLocalChanges(
      GetSpecificsFromInstalledApps(local_and_server_apps));

  syncer::EntityChangeList entity_changes;
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  // Serialize the web app before and after the update to verify no change has
  // occurred.
  std::unique_ptr<proto::WebApp> before_update =
      WebAppToProto(*registrar().GetAppById(local_and_server_apps[0]));

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  std::unique_ptr<proto::WebApp> after_update =
      WebAppToProto(*registrar().GetAppById(local_and_server_apps[0]));
  EXPECT_EQ(before_update->SerializeAsString(),
            after_update->SerializeAsString());

  EXPECT_EQ(registrar_registry().size(), 1ul);
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, ApplyIncrementalSyncChanges_AddUpdateDelete) {
  StartWebAppProvider();

  webapps::AppId app_id_to_keep = test::InstallDummyWebApp(
      profile(), "To Keep", GURL("https://www.to_keep.com/"),
      webapps::WebappInstallSource::SYNC);
  webapps::AppId app_id_to_delete = test::InstallDummyWebApp(
      profile(), "To Delete", GURL("https://www.to_delete.com/"),
      webapps::WebappInstallSource::SYNC);
  webapps::AppId app_id_to_update = test::InstallDummyWebApp(
      profile(), "To Update", GURL("https://www.to_update.com/"),
      webapps::WebappInstallSource::SYNC);

  WebAppSpecifics sync_app_to_add = CreateWebAppSpecificsForTesting(
      "To Add", GURL("https://www.to_add.com/"));
  webapps::AppId sync_app_to_add_id =
      GetAppIdFromWebAppSpecifics(sync_app_to_add);

  WebAppSpecifics sync_app_to_update = CreateWebAppSpecificsForTesting(
      "To Update, Updated", GURL("https://www.to_update.com/"));

  syncer::EntityChangeList entity_changes;

  // Add
  ConvertSpecificsToEntityChange(
      sync_app_to_add, syncer::EntityChange::ACTION_ADD, &entity_changes);

  // Update
  ConvertSpecificsToEntityChange(
      sync_app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);

  // Delete
  entity_changes.push_back(syncer::EntityChange::CreateDelete(
      app_id_to_delete, syncer::EntityData()));

  // There should be no changes sent to USS in the next
  // ApplyIncrementalSyncChanges() operation.
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  WebAppTestUninstallObserver app_uninstalled(profile());
  app_uninstalled.BeginListening({app_id_to_delete});

  WebAppTestInstallObserver app_added(profile());
  app_added.BeginListening({sync_app_to_add_id});

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  EXPECT_FALSE(app_uninstalled.Wait().empty());
  EXPECT_FALSE(app_added.Wait().empty());
  // Update doesn't have an observer, but it happens synchronously.

  // Check app added.
  ASSERT_TRUE(registrar().GetAppById(sync_app_to_add_id));
  EXPECT_EQ(sync_app_to_add.SerializeAsString(),
            registrar()
                .GetAppById(sync_app_to_add_id)
                ->sync_proto()
                .SerializeAsString());
  EXPECT_EQ(registrar().GetAppShortName(sync_app_to_add_id), "To Add");

  // Check app deleted.
  EXPECT_FALSE(registrar().GetAppById(app_id_to_delete));

  // Check app updated.
  ASSERT_TRUE(registrar().GetAppById(app_id_to_update));
  EXPECT_EQ(sync_app_to_update.SerializeAsString(),
            registrar()
                .GetAppById(app_id_to_update)
                ->sync_proto()
                .SerializeAsString());
  // Note - the name should NOT change, as that is not actively synced, and
  // should not normally be updated without user confirmation, so we save the
  // originally installed name.
  EXPECT_EQ(registrar().GetAppShortName(app_id_to_update), "To Update");

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_OverlappingOtherManagement) {
  StartWebAppProvider();
  // Test that all apps are also installed by the user (another management
  // type), and also install via sync a few. Verify that addition, update, and
  // deletion via sync appropriately updates but does not delete the apps, as
  // they are controlled by another management type too.

  // None of these will be installed with the SYNC management type yet.
  webapps::AppId app_id_to_add = test::InstallDummyWebApp(
      profile(), "To Keep", GURL("https://www.to_keep.com/"),
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  webapps::AppId app_id_to_delete = test::InstallDummyWebApp(
      profile(), "To Delete", GURL("https://www.to_delete.com/"),
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  webapps::AppId app_id_to_update = test::InstallDummyWebApp(
      profile(), "To Update", GURL("https://www.to_update.com/"),
      webapps::WebappInstallSource::EXTERNAL_DEFAULT);

  // Create what we plan on using with Sync
  WebAppSpecifics sync_app_to_add = GetSpecificsFromInstalledApp(app_id_to_add);
  WebAppSpecifics sync_app_to_update =
      GetSpecificsFromInstalledApp(app_id_to_update);
  // Currently the only field to have update propagate is display mode. This
  // might be removed in the future.
  SetPlatformSpecificUserDisplayMode(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER, &sync_app_to_update);
  WebAppSpecifics sync_app_to_delete =
      GetSpecificsFromInstalledApp(app_id_to_delete);

  // Set up initial sync management source state.
  syncer::EntityChangeList sync_data_list;
  ConvertAppToEntityChange(*registrar().GetAppById(app_id_to_update),
                           syncer::EntityChange::ChangeType::ACTION_ADD,
                           &sync_data_list);
  ConvertAppToEntityChange(*registrar().GetAppById(app_id_to_delete),
                           syncer::EntityChange::ChangeType::ACTION_ADD,
                           &sync_data_list);
  ON_CALL(processor(), Put).WillByDefault(testing::Return());
  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  std::move(sync_data_list));

  // Check initial state.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  std::vector<webapps::AppId> all_ids = {app_id_to_add, app_id_to_update,
                                         app_id_to_delete};
  for (const webapps::AppId& app_id : all_ids) {
    ASSERT_TRUE(
        registrar().AppMatches(app_id, WebAppFilter::InstalledInChrome()));
    EXPECT_TRUE(registrar().GetAppById(app_id)->GetSources().Has(
        WebAppManagement::kDefault));
    if (app_id != app_id_to_add) {
      EXPECT_TRUE(registrar().GetAppById(app_id)->GetSources().Has(
          WebAppManagement::kSync));
    }
  }

  // Do the add, update, and delete operations from sync.
  syncer::EntityChangeList entity_changes;
  ConvertSpecificsToEntityChange(
      sync_app_to_add, syncer::EntityChange::ACTION_ADD, &entity_changes);
  ConvertSpecificsToEntityChange(
      sync_app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
  entity_changes.push_back(syncer::EntityChange::CreateDelete(
      app_id_to_delete, syncer::EntityData()));

  // There should be no changes sent to USS in the next
  // ApplyIncrementalSyncChanges() operation.
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  WebAppInstallManagerObserverAdapter install_adapter(profile());
  base::test::TestFuture<const webapps::AppId&> source_removed_future;
  install_adapter.SetWebAppSourceRemovedDelegate(
      source_removed_future.GetRepeatingCallback());

  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  ASSERT_TRUE(source_removed_future.Wait());
  EXPECT_EQ(app_id_to_delete, source_removed_future.Get());
  // Note: Update happens synchronously, waiting for the above should be
  // enough.

  // Check app added.
  ASSERT_TRUE(registrar().GetAppById(app_id_to_add));
  EXPECT_TRUE(registrar().GetAppById(app_id_to_add)->IsSynced());

  // Check app deleted only had the sync source removed.
  ASSERT_TRUE(registrar().GetAppById(app_id_to_delete));
  EXPECT_FALSE(registrar().GetAppById(app_id_to_delete)->IsSynced());

  // Check app updated.
  ASSERT_TRUE(registrar().GetAppById(app_id_to_update));
  EXPECT_EQ(sync_app_to_update.SerializeAsString(),
            registrar()
                .GetAppById(app_id_to_update)
                ->sync_proto()
                .SerializeAsString());

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

// Commits local data (e.g. installed web apps) before sync is hooked up. This
// tests that the web apps are correctly sent to USS after MergeFullSyncData is
// called.
TEST_F(WebAppSyncBridgeTest, CommitUpdate_CommitWhileNotTrackingMetadata) {
  EXPECT_CALL(processor(), ModelReadyToSync(_)).Times(1);
  StartWebAppProvider();

  EXPECT_CALL(processor(), IsTrackingMetadata())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);

  // Install apps by user.
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "App", GURL("https://www.app.com/"),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  // Add sync source.
  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());
    update->UpdateApp(app_id)->AddSource(WebAppManagement::kSync);
  }
  ASSERT_TRUE(future.Take());
  testing::Mock::VerifyAndClear(&processor());

  // Turn on tracking, and merge, verifying the app gets uploaded.
  base::test::TestFuture<const std::string&,
                         std::unique_ptr<syncer::EntityData>,
                         syncer::MetadataChangeList*>
      put_future;
  EXPECT_CALL(processor(), Put).WillOnce(base::test::InvokeFuture(put_future));
  EXPECT_CALL(processor(), Delete).Times(0);
  EXPECT_CALL(processor(), IsTrackingMetadata())
      .WillOnce(testing::Return(true));

  sync_bridge().MergeFullSyncData(sync_bridge().CreateMetadataChangeList(),
                                  syncer::EntityChangeList{});
  ASSERT_TRUE(put_future.Wait());
  EXPECT_EQ(put_future.Get<0>(), app_id);

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

TEST_F(WebAppSyncBridgeTest, PutCalledOnUpdate) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const webapps::AppId kAppId = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(kStartUrl));

  test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_CALL(processor(), Put).WillOnce(testing::Return());
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://www.example.com/index.html"),
      webapps::WebappInstallSource::SYNC);

  base::test::TestFuture<const std::string&,
                         std::unique_ptr<syncer::EntityData>,
                         syncer::MetadataChangeList*>
      put_future;

  EXPECT_CALL(processor(), Put).WillOnce(base::test::InvokeFuture(put_future));

  // Changing the start_url, which should cause an update to the sync proto.
  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());
    update->UpdateApp(app_id)->SetStartUrl(
        GURL("https://www.example.com/index2.html"));
  }
  ASSERT_TRUE(future.Take());

  ASSERT_TRUE(put_future.Wait());
  EXPECT_EQ(put_future.Get<0>(), kAppId);

  WebAppSpecifics expected;
  expected.set_name("Test App");
  expected.set_start_url("https://www.example.com/index2.html");
  SetPlatformSpecificUserDisplayMode(WebAppSpecifics_UserDisplayMode_STANDALONE,
                                     &expected);
  expected.set_relative_manifest_id("index.html");
  expected.set_scope("https://www.example.com/");

  const WebAppSpecifics& specifics =
      put_future.Get<std::unique_ptr<syncer::EntityData>>()
          ->specifics.web_app();
  EXPECT_EQ(expected.SerializeAsString(), specifics.SerializeAsString());
}

TEST_F(WebAppSyncBridgeTest, DeleteCalledOnUninstall) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");

  test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_CALL(processor(), Put).WillOnce(testing::Return());
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://www.example.com/index.html"),
      webapps::WebappInstallSource::SYNC);

  base::test::TestFuture<const std::string&, const syncer::DeletionOrigin&,
                         syncer::MetadataChangeList*>
      delete_future;

  EXPECT_CALL(processor(), Delete(app_id, _, _))
      .WillOnce(base::test::InvokeFuture(delete_future));

  base::test::TestFuture<webapps::UninstallResultCode> result;
  fake_provider().scheduler().RemoveInstallManagementMaybeUninstall(
      app_id, WebAppManagement::kSync,
      webapps::WebappUninstallSource::kAppsPage, result.GetCallback());
  ASSERT_TRUE(result.Wait());
  EXPECT_EQ(result.Get(), webapps::UninstallResultCode::kAppRemoved);

  ASSERT_TRUE(delete_future.Wait());
  EXPECT_EQ(delete_future.Get<0>(), app_id);
}

TEST_F(WebAppSyncBridgeTest,
       CommitUpdate_DeleteSyncAppWithOverlappingExternalManagement) {
  StartWebAppProvider();

  // Install apps into sync.
  EXPECT_CALL(processor(), Put).Times(1);
  webapps::AppId app_id =
      test::InstallDummyWebApp(profile(), "App", GURL("https://www.app.com/"),
                               webapps::WebappInstallSource::SYNC);
  testing::Mock::VerifyAndClearExpectations(&processor());

  // Add user install source to both

  base::test::TestFuture<bool> future;
  {
    ScopedRegistryUpdate update =
        sync_bridge().BeginUpdate(future.GetCallback());
    update->UpdateApp(app_id)->AddSource(WebAppManagement::kUserInstalled);
  }
  ASSERT_TRUE(future.Take());

  // Remove the sync source.
  EXPECT_CALL(processor(), Delete(app_id, _, _)).Times(1);
  base::test::TestFuture<webapps::UninstallResultCode> result;
  provider().scheduler().RemoveInstallManagementMaybeUninstall(
      app_id, WebAppManagement::kSync,
      webapps::WebappUninstallSource::kAppsPage, result.GetCallback());

  ASSERT_TRUE(result.Wait());
  EXPECT_EQ(result.Get(), webapps::UninstallResultCode::kInstallSourceRemoved);

  ASSERT_TRUE(registrar().GetAppById(app_id));
  EXPECT_FALSE(registrar().GetAppById(app_id)->IsSynced());

  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar(
      /*exclude_current_os_integration=*/true));
}

// Test that any apps that are still pending install from sync (or,
// |is_from_sync_and_pending_installation|) are continued to be installed when
// the bridge initializes.
TEST_F(WebAppSyncBridgeTest, InstallAppsFromSyncAndPendingInstallation) {
  proto::WebApp web_app_proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  web_app_proto.set_is_from_sync_and_pending_installation(true);

  database_factory().WriteProtos({web_app_proto});
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({GetAppIdFromWebAppProto(web_app_proto)});

  StartWebAppProvider();

  webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(app_id, GetAppIdFromWebAppProto(web_app_proto));

  const WebApp* app = registrar().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_FALSE(app->is_from_sync_and_pending_installation());
}

// Tests that non user installable apps can also be removed by the
// WebAppSyncBridge during system startup, if `is_uninstalling` is set to true.
// Test for crbug.com/335253048, by using kSystem to mock that behavior. Since
// System Web Apps are only on Ash chrome, kPolicy is used instead on Lacro
TEST_F(WebAppSyncBridgeTest, CanDeleteNonUserInstallableApps) {
  proto::WebApp web_app_proto_uninstalling = CreateWebAppProtoForTesting(
      "UninstallingSystemApp", GURL("https://example.com/"));
  web_app_proto_uninstalling.mutable_sources()->set_system(true);
  web_app_proto_uninstalling.set_is_uninstalling(true);

  proto::WebApp web_app_proto =
      CreateWebAppProtoForTesting("System App", GURL("https://example2.com/"));
  web_app_proto.mutable_sources()->set_system(true);
  web_app_proto.mutable_sources()->set_user_installed(false);

  database_factory().WriteProtos({web_app_proto, web_app_proto_uninstalling});
  StartWebAppProvider();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(registrar().IsInRegistrar(
      GetAppIdFromWebAppProto(web_app_proto_uninstalling)));
  EXPECT_FALSE(registrar().AppMatches(
      GetAppIdFromWebAppProto(web_app_proto_uninstalling),
      WebAppFilter::InstalledInChrome()));

  EXPECT_TRUE(registrar().AppMatches(GetAppIdFromWebAppProto(web_app_proto),
                                     WebAppFilter::InstalledInChrome()));
}

// Tests that OnWebAppsWillBeUpdatedFromSync observer notification is called
// properly.
TEST_F(WebAppSyncBridgeTest,
       ApplyIncrementalSyncChanges_OnWebAppsWillBeUpdatedFromSync) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const webapps::AppId kAppId = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(kStartUrl));

  test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_CALL(processor(), Put).WillOnce(testing::Return());
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://www.example.com/index.html"),
      webapps::WebappInstallSource::SYNC);

  base::test::TestFuture<std::vector<webapps::AppId>> update_future;
  WebAppTestRegistryObserverAdapter observer{&registrar()};

  observer.SetWebAppWillBeUpdatedFromSyncDelegate(base::BindLambdaForTesting(
      [&](const std::vector<const WebApp*>& apps_to_update) {
        std::vector<webapps::AppId> app_ids;
        for (const WebApp* source : apps_to_update) {
          app_ids.push_back(source->app_id());
        }
        update_future.SetValue(app_ids);
      }));

  // Currently the only field to have update propagate is display mode. This
  // might be removed in the future.
  WebAppSpecifics sync_app_to_update = GetSpecificsFromInstalledApp(app_id);
  SetPlatformSpecificUserDisplayMode(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER, &sync_app_to_update);

  syncer::EntityChangeList entity_changes;
  ConvertSpecificsToEntityChange(
      sync_app_to_update, syncer::EntityChange::ACTION_UPDATE, &entity_changes);
  EXPECT_CALL(processor(), Put).Times(0);
  EXPECT_CALL(processor(), Delete).Times(0);
  sync_bridge().ApplyIncrementalSyncChanges(
      sync_bridge().CreateMetadataChangeList(), std::move(entity_changes));

  ASSERT_TRUE(update_future.Wait());
  EXPECT_THAT(update_future.Get(), testing::ElementsAre(app_id));
}

TEST_F(WebAppSyncBridgeTest, RetryIncompleteUninstalls) {
  proto::WebApp web_app_proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  web_app_proto.set_is_uninstalling(true);
  webapps::AppId app_id = GetAppIdFromWebAppProto(web_app_proto);

  database_factory().WriteProtos({web_app_proto});

  WebAppTestUninstallObserver uninstall_waiter(profile());
  uninstall_waiter.BeginListening({app_id});
  StartWebAppProvider();
  ASSERT_EQ(uninstall_waiter.Wait(), app_id);
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
  // WebAppSpecifics sync_proto;
  // sync_proto.set_start_url(kStartUrl);
  // sync_proto.set_name(kAppName);
  // sync_proto.set_user_display_mode_default(
  //     WebAppSpecifics::BROWSER);
  // sync_proto.set_test_new_field("hello");
  // sync_proto.SerializeAsString();
  const char kStartUrl[] = "https://example.com/launchurl";
  const std::string serialized_proto =
      "\n\035https://example.com/launchurl\022\tTest "
      "name\030\001\372\265\277\024\005hello";
  const GURL start_url = GURL(kStartUrl);
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);

  // Parse the proto.
  WebAppSpecifics sync_proto;
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
  WebAppSpecifics result_proto = app->sync_proto();
  result_proto.clear_relative_manifest_id();
#if BUILDFLAG(IS_CHROMEOS)
  result_proto.clear_user_display_mode_cros();
#endif

  // Check that the sync proto retained its value, including the unknown field.
  EXPECT_EQ(result_proto.SerializeAsString(), serialized_proto);
}

TEST_F(WebAppSyncBridgeTest, MigratePartiallyInstalledToCorrectStatus) {
  proto::WebApp web_app_proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  web_app_proto.set_install_state(
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);

  database_factory().WriteProtos({web_app_proto});
  StartWebAppProvider();

  for (const webapps::AppId& app_id : registrar().GetAppIds()) {
    EXPECT_EQ(registrar().GetAppById(app_id)->install_state(),
              proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  }
}

TEST_F(WebAppSyncBridgeTest, SyncOsIntegrationOnStartup) {
  base::HistogramTester histogram_tester;
  proto::WebApp web_app_proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  web_app_proto.set_install_state(
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  // Ensure OS integration state is missing.
  web_app_proto.clear_current_os_integration_states();
  webapps::AppId app_id = GetAppIdFromWebAppProto(web_app_proto);

  // Ensure the version is set so the install state doesn't get corrected.
  proto::DatabaseMetadata metadata;
  metadata.set_version(WebAppDatabase::GetCurrentDatabaseVersion());
  database_factory().WriteMetadata(metadata);
  database_factory().WriteProtos({web_app_proto});

  StartWebAppProvider();

  // Wait for the SynchronizeOsIntegration command triggered on startup to
  // complete.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Verify that the OS integration state is now populated in the registrar.
  const WebApp* app = registrar().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_TRUE(app->current_os_integration_states().has_shortcut());

  // Verify the histogram was recorded.
  histogram_tester.ExpectUniqueSample(
      "WebApp.Install.CompletedOsIntegrationOnStartup", true, 1);
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

  WebAppSyncBridgeTest_UserDisplayModeSplit()
#if BUILDFLAG(IS_CHROMEOS)
      // UDM mitigations mess with the installed local state, disable them so
      // the state matches the intention of the test.
      : disable_user_display_mode_sync_mitigations_for_testing_(
            &WebAppInstallFinalizer::
                DisableUserDisplayModeSyncMitigationsForTesting(),
            true)
#endif  // BUILDFLAG(IS_CHROMEOS)
  {
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
#if BUILDFLAG(IS_CHROMEOS)
  base::AutoReset<bool> disable_user_display_mode_sync_mitigations_for_testing_;
#endif  // BUILDFLAG(IS_CHROMEOS)
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
      WebAppSpecifics sync_proto = web_app->sync_proto();

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
  WebAppSpecifics sync_proto;
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

  std::optional<WebAppSpecifics::UserDisplayMode> app_this_platform_udm;
  std::optional<WebAppSpecifics::UserDisplayMode> app_other_platform_udm;
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

}  // namespace
}  // namespace web_app
