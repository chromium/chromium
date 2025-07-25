// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/generated_icon_fix_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_database_metadata.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_database_serialization.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

class WebAppDatabaseTest : public WebAppTest {
 public:
  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(mutable_registrar().registry(), registry);
  }

  void WriteBatch(
      std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch) {
    base::RunLoop run_loop;

    database_factory().GetStore()->CommitWriteBatch(
        std::move(write_batch),
        base::BindLambdaForTesting(
            [&](const std::optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Registry WriteWebApps(uint32_t num_apps,
                        bool only_non_external_management_types = false) {
    Registry registry;

    auto write_batch = database_factory().GetStore()->CreateWriteBatch();

    for (uint32_t i = 0; i < num_apps; ++i) {
      std::unique_ptr<WebApp> app =
          test::CreateRandomWebApp({.seed = i,
                                    .only_non_external_management_types =
                                        only_non_external_management_types});
      std::unique_ptr<proto::WebApp> proto = WebAppToProto(*app);
      const webapps::AppId app_id = app->app_id();

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(app));
    }
    proto::DatabaseMetadata metadata;
    metadata.set_version(WebAppDatabase::GetCurrentDatabaseVersion());
    write_batch->WriteData(std::string(WebAppDatabase::kDatabaseMetadataKey),
                           metadata.SerializeAsString());
    WriteBatch(std::move(write_batch));

    return registry;
  }

 protected:
  FakeWebAppDatabaseFactory& database_factory() {
    return *fake_provider().GetDatabaseFactory().AsFakeWebAppDatabaseFactory();
  }

  WebAppRegistrar& registrar() { return fake_provider().GetRegistrarMutable(); }

  WebAppRegistrarMutable& mutable_registrar() {
    return fake_provider().GetRegistrarMutable();
  }

  WebAppSyncBridge& sync_bridge() {
    return fake_provider().sync_bridge_unsafe();
  }

  void RegisterApp(std::unique_ptr<WebApp> web_app) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  void UnregisterApp(const webapps::AppId& app_id) {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->DeleteApp(app_id);
  }

  void UnregisterAll() {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    for (const webapps::AppId& app_id : registrar().GetAppIds()) {
      update->DeleteApp(app_id);
    }
  }
};

TEST_F(WebAppDatabaseTest, WriteAndReadRegistry) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  EXPECT_TRUE(registrar().is_empty());

  const uint32_t num_apps = 1000;

  std::unique_ptr<WebApp> app = test::CreateRandomWebApp({.seed = 0});
  webapps::AppId app_id = app->app_id();
  RegisterApp(std::move(app));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  for (uint32_t i = 1; i <= num_apps; ++i) {
    std::unique_ptr<WebApp> extra_app = test::CreateRandomWebApp({.seed = i});
    RegisterApp(std::move(extra_app));
  }
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  UnregisterApp(app_id);
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  UnregisterAll();
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppDatabaseTest, WriteAndDeleteAppsWithCallbacks) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  EXPECT_TRUE(registrar().is_empty());

  const uint32_t num_apps = 100;

  RegistryUpdateData::Apps apps_to_create;
  std::vector<webapps::AppId> apps_to_delete;
  Registry expected_registry;

#if BUILDFLAG(IS_CHROMEOS)
  bool allow_system_source = true;
#else
  bool allow_system_source = false;
#endif

  for (uint32_t i = 0; i < num_apps; ++i) {
    std::unique_ptr<WebApp> app = test::CreateRandomWebApp(
        {.seed = i, .allow_system_source = allow_system_source});
    apps_to_delete.push_back(app->app_id());
    apps_to_create.push_back(std::move(app));

    std::unique_ptr<WebApp> expected_app = test::CreateRandomWebApp(
        {.seed = i, .allow_system_source = allow_system_source});
    expected_registry.emplace(expected_app->app_id(), std::move(expected_app));
  }

  {
    base::test::TestFuture<bool> future;
    {
      ScopedRegistryUpdate update =
          sync_bridge().BeginUpdate(future.GetCallback());
      for (std::unique_ptr<WebApp>& web_app : apps_to_create) {
        update->CreateApp(std::move(web_app));
      }
    }
    EXPECT_TRUE(future.Take());

    Registry registry_written = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(registry_written, expected_registry));
  }

  {
    base::test::TestFuture<bool> future;
    {
      ScopedRegistryUpdate update =
          sync_bridge().BeginUpdate(future.GetCallback());
      for (const webapps::AppId& app_id : apps_to_delete) {
        update->DeleteApp(app_id);
      }
    }
    EXPECT_TRUE(future.Take());

    Registry registry_deleted = database_factory().ReadRegistry();
    EXPECT_TRUE(registry_deleted.empty());
  }
}

// Read a database where all apps are already in a valid state, so there should
// be no difference between the apps written and read.
TEST_F(WebAppDatabaseTest, OpenDatabaseAndReadRegistry) {
  constexpr int kNumApps = 20;
  auto disable_sync_install_and_missing_os_integration = WebAppSyncBridge::
      DisableResumeSyncInstallAndMissingOsIntegrationForTesting();
  auto disable_generated_icon_fixes =
      GeneratedIconFixManager::DisableGeneratedIconFixesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
  // Some random apps will be configured to run on login, and by doing so we
  // will 'fix' the InstallState if the OS integration is not there. So disable
  // this behavior to prevent this from occurring.
  auto disable_run_on_os_login =
      WebAppRunOnOsLoginManager::SkipStartupForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::HistogramTester histogram_tester;
  Registry registry =
      WriteWebApps(kNumApps, /*only_non_external_management_types=*/true);
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  histogram_tester.ExpectBucketCount("WebApp.Database.ValidProto", true,
                                     kNumApps);
  histogram_tester.ExpectBucketCount("WebApp.Database.AppIdMatch", true,
                                     kNumApps);
  fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_TRUE(IsRegistryEqual(database_factory().ReadRegistry(), registry,
                              /*exclude_current_os_integration=*/true));
  EXPECT_EQ(database_factory().ReadMetadata().version(),
            WebAppDatabase::GetCurrentDatabaseVersion());
}

// Tests handling crashes fixed in crbug.com/1417955.
TEST_F(WebAppDatabaseTest, MigrateFromMissingShortcutsSizes) {
  std::unique_ptr<WebApp> base_app = test::CreateRandomWebApp({});
  WebAppShortcutsMenuItemInfo shortcut_item_info{};
  shortcut_item_info.name = u"shortcut";
  shortcut_item_info.url = GURL("http://example.com/shortcut");
  shortcut_item_info.downloaded_icon_sizes.any = {42};
  shortcut_item_info.downloaded_icon_sizes.maskable = {24};
  shortcut_item_info.downloaded_icon_sizes.monochrome = {123};
  base_app->SetShortcutsMenuInfo({shortcut_item_info});

  std::unique_ptr<proto::WebApp> base_proto = WebAppToProto(*base_app);

  proto::WebApp proto_without_shortcut_info(*base_proto);
  proto_without_shortcut_info.clear_shortcuts_menu_item_infos();
  // Fail to parse when fewer shortcut infos than downloaded sizes. No evidence
  // this happens in the wild.
  EXPECT_EQ(ParseWebAppProto(proto_without_shortcut_info), nullptr);

  // If DB is missing downloaded shortcut icon sizes information, expect to pad
  // the vector with empty IconSizes structs so the vectors in WebApp have equal
  // length.
  proto::WebApp proto_without_downloaded_sizes(*base_proto);
  proto_without_downloaded_sizes.clear_downloaded_shortcuts_menu_icons_sizes();
  auto roundtrip_app = ParseWebAppProto(proto_without_downloaded_sizes);

  auto app_with_empty_downloaded_sizes = std::make_unique<WebApp>(*base_app);
  shortcut_item_info.downloaded_icon_sizes = {};
  app_with_empty_downloaded_sizes->SetShortcutsMenuInfo({shortcut_item_info});

  EXPECT_EQ(base::ToString(*roundtrip_app),
            base::ToString(*app_with_empty_downloaded_sizes));
}

}  // namespace web_app
