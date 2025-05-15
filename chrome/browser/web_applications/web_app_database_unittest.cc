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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
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
#include "components/webapps/isolated_web_apps/update_channel.h"
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

TEST_F(WebAppDatabaseTest, BackwardCompatibility_WebAppWithOnlyRequiredFields) {
  const GURL start_url{"https://example.com/"};
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  const std::string name = "App Name";

  std::vector<std::unique_ptr<proto::WebApp>> protos;

  // Create a proto with |required| only fields.
  // Do not add new fields in this test: any new fields should be |optional|.
  auto proto = std::make_unique<proto::WebApp>();
  {
    sync_pb::WebAppSpecifics sync_proto;
    sync_proto.set_start_url(start_url.spec());
    sync_proto.set_user_display_mode_default(
        sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
    *(proto->mutable_sync_data()) = std::move(sync_proto);
  }

  proto->set_name(name);
  proto->set_install_state(proto::INSTALLED_WITH_OS_INTEGRATION);

  proto->mutable_sources()->set_system(false);
  proto->mutable_sources()->set_policy(false);
  proto->mutable_sources()->set_web_app_store(false);
  proto->mutable_sources()->set_sync(true);
  proto->mutable_sources()->set_default_(false);

  if (IsChromeOsDataMandatory()) {
    proto->mutable_chromeos_data()->set_show_in_launcher(false);
    proto->mutable_chromeos_data()->set_show_in_search_and_shelf(false);
    proto->mutable_chromeos_data()->set_show_in_management(false);
    proto->mutable_chromeos_data()->set_is_disabled(true);
  }

  protos.push_back(std::move(proto));
  database_factory().WriteProtos(protos);

  // Read the registry: the proto parsing may fail while reading the proto
  // above.
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(start_url, app->start_url());
  EXPECT_EQ(name, app->untranslated_name());
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser, app->user_display_mode());
  EXPECT_EQ(proto::INSTALLED_WITHOUT_OS_INTEGRATION, app->install_state());
  EXPECT_TRUE(app->IsSynced());
  EXPECT_TRUE(app->GetSources().Has(WebAppManagement::kUserInstalled));
  EXPECT_FALSE(app->IsPreinstalledApp());

  if (IsChromeOsDataMandatory()) {
    EXPECT_FALSE(app->chromeos_data()->show_in_launcher);
    EXPECT_FALSE(app->chromeos_data()->show_in_search_and_shelf);
    EXPECT_FALSE(app->chromeos_data()->show_in_management);
    EXPECT_TRUE(app->chromeos_data()->is_disabled);
  } else {
    EXPECT_FALSE(app->chromeos_data().has_value());
  }
}

TEST_F(WebAppDatabaseTest, WebAppWithManyIcons) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  const GURL base_url("https://example.com/path");
  // A number of icons of each IconPurpose.
  const int num_icons = 32;

  std::unique_ptr<WebApp> app =
      test::CreateRandomWebApp({.base_url = base_url});
  webapps::AppId app_id = app->app_id();

  std::vector<apps::IconInfo> icons;

  for (IconPurpose purpose : kIconPurposes) {
    std::vector<SquareSizePx> sizes;
    for (int i = 1; i <= num_icons; ++i) {
      apps::IconInfo icon;
      icon.url = base_url.Resolve("icon" + base::NumberToString(num_icons));
      // Let size equals the icon's number squared.
      icon.square_size_px = i * i;

      icon.purpose = ManifestPurposeToIconInfoPurpose(purpose);
      sizes.push_back(*icon.square_size_px);
      icons.push_back(std::move(icon));
    }

    app->SetDownloadedIconSizes(purpose, std::move(sizes));
  }

  app->SetManifestIcons(std::move(icons));
  app->SetIsGeneratedIcon(false);

  RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);
  EXPECT_EQ(static_cast<unsigned>(num_icons * kIconPurposes.size()),
            app_copy->manifest_icons().size());
  for (int i = 1; i <= num_icons; ++i) {
    const int icon_size_in_px = i * i;
    EXPECT_EQ(icon_size_in_px,
              app_copy->manifest_icons()[i - 1].square_size_px);
  }
  EXPECT_FALSE(app_copy->is_generated_icon());
}

TEST_F(WebAppDatabaseTest, MigrateOldLaunchHandlerSyntax) {
  std::unique_ptr<WebApp> base_app = test::CreateRandomWebApp({});
  std::unique_ptr<proto::WebApp> base_proto = WebAppToProto(*base_app);

  // "launch_handler": {
  //   "route_to": "existing-client",
  //   "navigate_existing_client": "always"
  // }
  // ->
  // "launch_handler": {
  //   "client_mode": "navigate-existing"
  // }
  proto::WebApp old_navigate_proto(*base_proto);
  old_navigate_proto.mutable_launch_handler()->set_route_to(
      proto::LaunchHandler_DeprecatedRouteTo_EXISTING_CLIENT);
  old_navigate_proto.mutable_launch_handler()->set_navigate_existing_client(
      proto::LaunchHandler_DeprecatedNavigateExistingClient_ALWAYS);
  old_navigate_proto.mutable_launch_handler()->set_client_mode(
      proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);

  std::unique_ptr<WebApp> new_navigate_app =
      ParseWebAppProto(old_navigate_proto);
  EXPECT_EQ(LaunchHandler::ClientMode::kNavigateExisting,
            new_navigate_app->launch_handler()->parsed_client_mode())
      << new_navigate_app->launch_handler()->parsed_client_mode();
  EXPECT_TRUE(
      new_navigate_app->launch_handler()->client_mode_valid_and_specified());

  std::unique_ptr<proto::WebApp> new_navigate_proto =
      WebAppToProto(*new_navigate_app);
  EXPECT_EQ(new_navigate_proto->launch_handler().route_to(),
            proto::LaunchHandler_DeprecatedRouteTo_UNSPECIFIED_ROUTE);
  EXPECT_EQ(
      new_navigate_proto->launch_handler().navigate_existing_client(),
      proto::
          LaunchHandler_DeprecatedNavigateExistingClient_UNSPECIFIED_NAVIGATE);
  EXPECT_EQ(new_navigate_proto->launch_handler().client_mode(),
            proto::LaunchHandler::CLIENT_MODE_NAVIGATE_EXISTING);

  // "launch_handler": {
  //   "route_to": "existing-client",
  //   "navigate_existing_client": "never"
  // }
  // ->
  // "launch_handler": {
  //   "client_mode": "focus-existing"
  // }
  proto::WebApp old_focus_proto(*base_proto);
  old_focus_proto.mutable_launch_handler()->set_route_to(
      proto::LaunchHandler_DeprecatedRouteTo_EXISTING_CLIENT);
  old_focus_proto.mutable_launch_handler()->set_navigate_existing_client(
      proto::LaunchHandler_DeprecatedNavigateExistingClient_NEVER);
  old_focus_proto.mutable_launch_handler()->set_client_mode(
      proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);

  std::unique_ptr<WebApp> new_focus_app = ParseWebAppProto(old_focus_proto);

  EXPECT_EQ(LaunchHandler::ClientMode::kFocusExisting,
            new_focus_app->launch_handler()->parsed_client_mode())
      << new_focus_app->launch_handler()->parsed_client_mode();
  EXPECT_TRUE(
      new_focus_app->launch_handler()->client_mode_valid_and_specified());

  std::unique_ptr<proto::WebApp> new_focus_proto =
      WebAppToProto(*new_focus_app);
  EXPECT_EQ(new_focus_proto->launch_handler().route_to(),
            proto::LaunchHandler_DeprecatedRouteTo_UNSPECIFIED_ROUTE);
  EXPECT_EQ(
      new_focus_proto->launch_handler().navigate_existing_client(),
      proto::
          LaunchHandler_DeprecatedNavigateExistingClient_UNSPECIFIED_NAVIGATE);
  EXPECT_EQ(new_focus_proto->launch_handler().client_mode(),
            proto::LaunchHandler::CLIENT_MODE_FOCUS_EXISTING);
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

// Old versions of Chrome may have stored sync data with a manifest_id_path
// containing a fragment part in the URL. It should be stripped out, because the
// spec requires that ManifestIds with different fragments are considered
// equivalent.
TEST_F(WebAppDatabaseTest, RemovesFragmentFromSyncProtoManifestIdPath) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<WebApp> app = test::CreateRandomWebApp({});
  // Apps must always have a valid manifest ID without a ref.
  EXPECT_TRUE(app->manifest_id().is_valid());
  EXPECT_FALSE(app->manifest_id().has_ref());
  std::string relative_manifest_id_path =
      app->sync_proto().relative_manifest_id();

  std::unique_ptr<proto::WebApp> proto = WebAppToProto(*app);
  proto->mutable_sync_data()->set_relative_manifest_id(
      relative_manifest_id_path + "#fragment");
  EXPECT_EQ(proto->sync_data().relative_manifest_id(),
            relative_manifest_id_path + "#fragment");

  // Re-parse the app from the proto.
  auto roundtrip_app = ParseWebAppProto(*proto);
  ASSERT_TRUE(roundtrip_app);

  // Loaded app should have had the fragment stripped.
  EXPECT_EQ(roundtrip_app->sync_proto().relative_manifest_id(),
            relative_manifest_id_path);
  EXPECT_FALSE(roundtrip_app->manifest_id().has_ref());

  histogram_tester.ExpectUniqueSample("WebApp.ParseWebAppProto.ManifestIdMatch",
                                      false, 1);
}

TEST_F(WebAppDatabaseTest, RemovesFragmentAndQueriesFromScopeDuringParsing) {
  std::unique_ptr<WebApp> app = test::CreateRandomWebApp({});
  EXPECT_TRUE(app->scope().is_valid());
  EXPECT_FALSE(app->scope().has_ref());
  std::string basic_scope_path = app->scope().spec();
  std::string scope_path_with_queries_and_fragment =
      base::StrCat({basic_scope_path, "?query=abc", "fragment"});

  // Create a proto::WebApp with a scope that has queries and fragments.
  std::unique_ptr<proto::WebApp> proto = WebAppToProto(*app);
  proto->set_scope(scope_path_with_queries_and_fragment);
  EXPECT_EQ(proto->scope(), scope_path_with_queries_and_fragment);

  // Re-parse the app from the proto.
  auto reparsed_app = ParseWebAppProto(*proto);
  ASSERT_TRUE(reparsed_app);

  // Loaded app should have had the fragment and query stripped.
  EXPECT_EQ(reparsed_app->scope(), basic_scope_path);
  EXPECT_FALSE(reparsed_app->scope().has_ref());
  EXPECT_FALSE(reparsed_app->scope().has_query());
}

}  // namespace web_app
