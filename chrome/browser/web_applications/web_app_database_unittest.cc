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
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
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
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

class WebAppDatabaseTest : public base::test::WithFeatureOverride,
                           public WebAppTest {
 public:
  WebAppDatabaseTest()
      : base::test::WithFeatureOverride(
            features::kWebAppDontAddExistingAppsToSync) {}

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());

    auto sync_bridge = std::make_unique<WebAppSyncBridge>(
        &provider_->GetRegistrarMutable(),
        mock_processor_.CreateForwardingProcessor());
    sync_bridge_ = sync_bridge.get();

    auto database_factory = std::make_unique<FakeWebAppDatabaseFactory>();
    database_factory_ = database_factory.get();

    provider_->SetDatabaseFactory(std::move(database_factory));
    provider_->SetSyncBridge(std::move(sync_bridge));

    sync_bridge_->SetSubsystems(
        database_factory_, &provider_->GetCommandManager(),
        &provider_->scheduler(), &provider_->GetInstallManager());

    provider_->Start();
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

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

  Registry WriteWebApps(uint32_t num_apps, bool ensure_no_migration_needed) {
    Registry registry;

    auto write_batch = database_factory().GetStore()->CreateWriteBatch();

    for (uint32_t i = 0; i < num_apps; ++i) {
      std::unique_ptr<WebApp> app = test::CreateRandomWebApp({.seed = i});
      if (ensure_no_migration_needed) {
        EnsureHasUserDisplayModeForCurrentPlatform(*app);
        if (base::FeatureList::IsEnabled(
                features::kWebAppDontAddExistingAppsToSync)) {
          if (app->GetSources().Has(WebAppManagement::kSync)) {
            app->AddSource(WebAppManagement::kUserInstalled);
          }
        }
        proto::DatabaseMetadata metadata;
        metadata.set_version(WebAppDatabase::GetCurrentDatabaseVersion());
        write_batch->WriteData(
            std::string(WebAppDatabase::kDatabaseMetadataKey),
            metadata.SerializeAsString());
      }
      std::unique_ptr<WebAppProto> proto =
          WebAppDatabase::CreateWebAppProto(*app);
      const webapps::AppId app_id = app->app_id();

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(app));
    }

    WriteBatch(std::move(write_batch));

    return registry;
  }

  void EnsureHasUserDisplayModeForCurrentPlatform(WebApp& app) {
    // Avoid using `WebApp::user_display_mode` because it DCHECKs for a valid
    // UDM.
#if BUILDFLAG(IS_CHROMEOS)
    if (app.sync_proto().has_user_display_mode_cros()) {
      return;
    }
#else
    if (app.sync_proto().has_user_display_mode_default()) {
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    app.SetUserDisplayMode(ToMojomUserDisplayMode(
        app.sync_proto().has_user_display_mode_default()
            ? app.sync_proto().user_display_mode_default()
            : sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE));
  }

 protected:
  FakeWebAppDatabaseFactory& database_factory() { return *database_factory_; }

  WebAppRegistrar& registrar() { return provider_->GetRegistrarMutable(); }

  WebAppRegistrarMutable& mutable_registrar() {
    return provider_->GetRegistrarMutable();
  }

  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }

  void InitSyncBridge() {
    base::RunLoop loop;
    sync_bridge_->Init(loop.QuitClosure());
    loop.Run();
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

 private:
  raw_ptr<WebAppSyncBridge, DanglingUntriaged> sync_bridge_ = nullptr;
  raw_ptr<FakeWebAppDatabaseFactory, DanglingUntriaged> database_factory_ =
      nullptr;
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  base::test::ScopedFeatureList feature_list_;

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
};

TEST_P(WebAppDatabaseTest, WriteAndReadRegistry) {
  InitSyncBridge();
  EXPECT_TRUE(registrar().is_empty());

  const uint32_t num_apps = 100;

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

TEST_P(WebAppDatabaseTest, WriteAndDeleteAppsWithCallbacks) {
  InitSyncBridge();
  EXPECT_TRUE(registrar().is_empty());

  const uint32_t num_apps = 100;

  RegistryUpdateData::Apps apps_to_create;
  std::vector<webapps::AppId> apps_to_delete;
  Registry expected_registry;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
TEST_P(WebAppDatabaseTest, OpenDatabaseAndReadRegistry) {
  Registry registry = WriteWebApps(100, /*ensure_no_migration_needed=*/true);

  InitSyncBridge();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
  EXPECT_TRUE(IsRegistryEqual(database_factory().ReadRegistry(), registry));
  EXPECT_EQ(database_factory().ReadMetadata().version(),
            WebAppDatabase::GetCurrentDatabaseVersion());
}

// Read a database where some apps will be migrated at read time.
TEST_P(WebAppDatabaseTest, OpenDatabaseAndReadRegistryWithMigration) {
  Registry registry = WriteWebApps(100, /*ensure_no_migration_needed=*/false);

  InitSyncBridge();

  EXPECT_EQ(database_factory().ReadMetadata().version(),
            WebAppDatabase::GetCurrentDatabaseVersion());

  // Some apps should have been migrated from an invalid state (missing
  // UserDisplayMode setting for the current platform) at read time.
  EXPECT_FALSE(IsRegistryEqual(mutable_registrar().registry(), registry));

  // Update the registry so apps reflect expected migrated state.
  for (auto& [app_id, app] : registry) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // System Web Apps are ignored by the registry on Lacros.
    if (app->IsSystemApp()) {
      continue;
    }
#endif
    EnsureHasUserDisplayModeForCurrentPlatform(*app);

    if (base::FeatureList::IsEnabled(
            features::kWebAppDontAddExistingAppsToSync)) {
      if (app->GetSources().Has(WebAppManagement::kSync)) {
        app->AddSource(WebAppManagement::kUserInstalled);
      }
    }
  }

  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
  EXPECT_TRUE(IsRegistryEqual(database_factory().ReadRegistry(), registry));
}

// Read a database where some apps will be migrated from not having a
// kUserInstalled source to having one.
TEST_P(WebAppDatabaseTest,
       OpenDatabaseAndReadRegistryWithSourceUpgradeMigration) {
  Registry registry = WriteWebApps(100, /*ensure_no_migration_needed=*/false);
  auto write_batch = database_factory().GetStore()->CreateWriteBatch();
  proto::DatabaseMetadata metadata;
  metadata.set_version(0);
  write_batch->WriteData(std::string(WebAppDatabase::kDatabaseMetadataKey),
                         metadata.SerializeAsString());
  WriteBatch(std::move(write_batch));

  InitSyncBridge();

  EXPECT_EQ(database_factory().ReadMetadata().version(),
            WebAppDatabase::GetCurrentDatabaseVersion());

  bool found_migrated_apps = false;
  // Some apps should not have a kUserInstalled source before migration but will
  // have one after.
  for (auto& [app_id, app] : registry) {
    if (app->GetSources().Has(WebAppManagement::kSync)) {
      found_migrated_apps |=
          !app->GetSources().Has(WebAppManagement::kUserInstalled);
      if (base::FeatureList::IsEnabled(
              features::kWebAppDontAddExistingAppsToSync)) {
        EXPECT_TRUE(registrar().GetAppById(app_id)->GetSources().Has(
            WebAppManagement::kUserInstalled));
        EXPECT_TRUE(registrar().GetAppById(app_id)->GetSources().Has(
            WebAppManagement::kSync));
      }
    }
  }
  EXPECT_TRUE(found_migrated_apps)
      << "Generated apps did not include any that needed migrating.";
}

// Read a database where some apps will be migrated from not having a
// kUserInstalled source to having one. Additionally with sync disabled, the
// sync source should be removed.
TEST_P(WebAppDatabaseTest,
       OpenDatabaseAndReadRegistryWithSourceUpgradeMigrationNoSync) {
  Registry registry = WriteWebApps(100, /*ensure_no_migration_needed=*/false);
  auto write_batch = database_factory().GetStore()->CreateWriteBatch();
  proto::DatabaseMetadata metadata;
  metadata.set_version(0);
  write_batch->WriteData(std::string(WebAppDatabase::kDatabaseMetadataKey),
                         metadata.SerializeAsString());
  WriteBatch(std::move(write_batch));

  database_factory().set_is_syncing_apps(false);
  InitSyncBridge();

  bool found_migrated_apps = false;
  // Some apps should not have a kUserInstalled source before migration but will
  // have one after.
  for (auto& [app_id, app] : registry) {
    if (app->GetSources().Has(WebAppManagement::kSync)) {
      found_migrated_apps |=
          !app->GetSources().Has(WebAppManagement::kUserInstalled);
      if (base::FeatureList::IsEnabled(
              features::kWebAppDontAddExistingAppsToSync)) {
        EXPECT_TRUE(registrar().GetAppById(app_id)->GetSources().Has(
            WebAppManagement::kUserInstalled));
        EXPECT_FALSE(registrar().GetAppById(app_id)->GetSources().Has(
            WebAppManagement::kSync));
      }
    }
  }
  EXPECT_TRUE(found_migrated_apps)
      << "Generated apps did not include any that needed migrating.";
}

// Read a database where some apps will be migrated from having a kUserInstalled
// source to not having one.
TEST_P(WebAppDatabaseTest,
       OpenDatabaseAndReadRegistryWithSourceDowngradeMigration) {
  Registry registry = WriteWebApps(100, /*ensure_no_migration_needed=*/false);
  auto write_batch = database_factory().GetStore()->CreateWriteBatch();
  proto::DatabaseMetadata metadata;
  metadata.set_version(1);
  write_batch->WriteData(std::string(WebAppDatabase::kDatabaseMetadataKey),
                         metadata.SerializeAsString());
  WriteBatch(std::move(write_batch));

  InitSyncBridge();

  EXPECT_EQ(database_factory().ReadMetadata().version(),
            WebAppDatabase::GetCurrentDatabaseVersion());

  bool found_migrated_apps = false;
  // Some apps should have a kUserInstalled source before migration but not
  // after.
  for (auto& [app_id, app] : registry) {
    if (app->GetSources().Has(WebAppManagement::kUserInstalled)) {
      found_migrated_apps = true;
      if (base::FeatureList::IsEnabled(
              features::kWebAppDontAddExistingAppsToSync)) {
        EXPECT_EQ(registrar().GetAppById(app_id)->GetSources(),
                  app->GetSources());
      } else {
        EXPECT_FALSE(registrar().GetAppById(app_id)->GetSources().Has(
            WebAppManagement::kUserInstalled));
        EXPECT_TRUE(registrar().GetAppById(app_id)->GetSources().Has(
            WebAppManagement::kSync));
      }
    }
  }
  EXPECT_TRUE(found_migrated_apps)
      << "Generated apps did not include any that needed migrating.";
}

TEST_P(WebAppDatabaseTest, BackwardCompatibility_WebAppWithOnlyRequiredFields) {
  const GURL start_url{"https://example.com/"};
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  const std::string name = "App Name";

  std::vector<std::unique_ptr<WebAppProto>> protos;

  // Create a proto with |required| only fields.
  // Do not add new fields in this test: any new fields should be |optional|.
  auto proto = std::make_unique<WebAppProto>();
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
  InitSyncBridge();

  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(start_url, app->start_url());
  EXPECT_EQ(name, app->untranslated_name());
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser, app->user_display_mode());
  EXPECT_EQ(proto::INSTALLED_WITHOUT_OS_INTEGRATION, app->install_state());
  EXPECT_TRUE(app->IsSynced());
  if (base::FeatureList::IsEnabled(
          features::kWebAppDontAddExistingAppsToSync)) {
    EXPECT_TRUE(app->GetSources().Has(WebAppManagement::kUserInstalled));
  }
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

TEST_P(WebAppDatabaseTest, UserDisplayModeCrosOnly_MigratesToCurrentPlatform) {
  std::unique_ptr<WebApp> base_app = test::CreateRandomWebApp({});
  std::unique_ptr<WebAppProto> base_proto =
      WebAppDatabase::CreateWebAppProto(*base_app);

  base_proto->mutable_sync_data()->set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  base_proto->mutable_sync_data()->clear_user_display_mode_default();

  std::vector<std::unique_ptr<WebAppProto>> protos;
  protos.push_back(std::move(base_proto));
  database_factory().WriteProtos(protos);

  InitSyncBridge();

  const WebApp* app = registrar().GetAppById(base_app->app_id());
  std::unique_ptr<WebAppProto> new_proto =
      WebAppDatabase::CreateWebAppProto(*app);

#if BUILDFLAG(IS_CHROMEOS)
  // On CrOS, the default field should remain absent.
  EXPECT_EQ(new_proto->sync_data().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  EXPECT_FALSE(new_proto->sync_data().has_user_display_mode_default());
  EXPECT_EQ(app->user_display_mode(), mojom::UserDisplayMode::kBrowser);
#else
  // On non-CrOS, both platform's fields should now be populated.
  EXPECT_EQ(new_proto->sync_data().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  // Default value doesn't migrate from CrOS value so should fall back to
  // standalone.
  EXPECT_EQ(new_proto->sync_data().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
  EXPECT_EQ(app->user_display_mode(), mojom::UserDisplayMode::kStandalone);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_P(WebAppDatabaseTest,
       UserDisplayModeDefaultOnly_MigratesToCurrentPlatform) {
  std::unique_ptr<WebApp> base_app = test::CreateRandomWebApp({});
  std::unique_ptr<WebAppProto> base_proto =
      WebAppDatabase::CreateWebAppProto(*base_app);

  base_proto->mutable_sync_data()->set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  base_proto->mutable_sync_data()->clear_user_display_mode_cros();

  std::vector<std::unique_ptr<WebAppProto>> protos;
  protos.push_back(std::move(base_proto));
  database_factory().WriteProtos(protos);

  InitSyncBridge();

  const WebApp* app = registrar().GetAppById(base_app->app_id());

  // Regardless of platform, the current platform's UDM should be set: the
  // default value should have been migrated in CrOS.
  EXPECT_EQ(app->user_display_mode(), mojom::UserDisplayMode::kBrowser);

  std::unique_ptr<WebAppProto> new_proto =
      WebAppDatabase::CreateWebAppProto(*app);

#if BUILDFLAG(IS_CHROMEOS)
  // On CrOS, both platform's fields should now be populated.
  EXPECT_EQ(new_proto->sync_data().user_display_mode_cros(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
  EXPECT_EQ(new_proto->sync_data().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
#else
  // On non-CrOS, the CrOS field should remain absent.
  EXPECT_FALSE(new_proto->sync_data().has_user_display_mode_cros());
  EXPECT_EQ(new_proto->sync_data().user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_P(WebAppDatabaseTest, WebAppWithoutOptionalFields) {
  InitSyncBridge();

  const auto start_url = GURL("https://example.com/");
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(start_url));
  const std::string name = "Name";

  auto app = std::make_unique<WebApp>(app_id);

  // Required fields:
  app->SetStartUrl(start_url);
  app->SetManifestId(GenerateManifestIdFromStartUrlOnly(start_url));
  app->SetName(name);
  app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  app->SetInstallState(proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
  // chromeos_data should always be set on ChromeOS.
  if (IsChromeOsDataMandatory())
    app->SetWebAppChromeOsData(std::make_optional<WebAppChromeOsData>());

  EXPECT_FALSE(app->HasAnySources());
  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    app->AddSource(type);
    EXPECT_TRUE(app->HasAnySources());
  }

  // Let optional fields be empty:
  EXPECT_EQ(app->display_mode(), DisplayMode::kUndefined);
  EXPECT_TRUE(app->display_mode_override().empty());
  EXPECT_TRUE(app->untranslated_description().empty());
  EXPECT_TRUE(app->scope().is_empty());
  EXPECT_FALSE(app->theme_color().has_value());
  EXPECT_FALSE(app->dark_mode_theme_color().has_value());
  EXPECT_FALSE(app->background_color().has_value());
  EXPECT_FALSE(app->dark_mode_background_color().has_value());
  EXPECT_TRUE(app->manifest_icons().empty());
  EXPECT_TRUE(app->downloaded_icon_sizes(IconPurpose::ANY).empty());
  EXPECT_TRUE(app->downloaded_icon_sizes(IconPurpose::MASKABLE).empty());
  EXPECT_TRUE(app->downloaded_icon_sizes(IconPurpose::MONOCHROME).empty());
  EXPECT_FALSE(app->is_generated_icon());
  EXPECT_FALSE(app->is_from_sync_and_pending_installation());
  EXPECT_FALSE(app->sync_proto().has_name());
  EXPECT_FALSE(app->sync_proto().has_theme_color());
  EXPECT_FALSE(app->sync_proto().has_scope());
  EXPECT_EQ(app->sync_proto().icon_infos_size(), 0);
  EXPECT_TRUE(app->file_handlers().empty());
  EXPECT_FALSE(app->share_target().has_value());
  EXPECT_TRUE(app->additional_search_terms().empty());
  EXPECT_TRUE(app->protocol_handlers().empty());
  EXPECT_TRUE(app->allowed_launch_protocols().empty());
  EXPECT_TRUE(app->disallowed_launch_protocols().empty());
  EXPECT_TRUE(app->url_handlers().empty());
  EXPECT_TRUE(app->scope_extensions().empty());
  EXPECT_TRUE(app->validated_scope_extensions().empty());
  EXPECT_TRUE(app->last_badging_time().is_null());
  EXPECT_TRUE(app->last_launch_time().is_null());
  EXPECT_TRUE(app->first_install_time().is_null());
  EXPECT_TRUE(app->shortcuts_menu_item_infos().empty());
  EXPECT_EQ(app->run_on_os_login_mode(), RunOnOsLoginMode::kNotRun);
  EXPECT_TRUE(app->manifest_url().is_empty());
  EXPECT_TRUE(app->permissions_policy().empty());
  EXPECT_FALSE(app->isolation_data().has_value());
  EXPECT_TRUE(app->latest_install_time().is_null());

  RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);

  // Required fields were serialized:
  EXPECT_EQ(app_id, app_copy->app_id());
  EXPECT_EQ(GenerateManifestIdFromStartUrlOnly(start_url),
            app_copy->manifest_id());
  EXPECT_EQ(start_url, app_copy->start_url());
  EXPECT_EQ(name, app_copy->untranslated_name());
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser, app_copy->user_display_mode());
  EXPECT_EQ(proto::SUGGESTED_FROM_ANOTHER_DEVICE, app_copy->install_state());

  auto& chromeos_data = app_copy->chromeos_data();
  if (IsChromeOsDataMandatory()) {
    EXPECT_TRUE(chromeos_data->show_in_launcher);
    EXPECT_TRUE(chromeos_data->show_in_search_and_shelf);
    EXPECT_TRUE(chromeos_data->show_in_management);
    EXPECT_FALSE(chromeos_data->is_disabled);
    EXPECT_FALSE(chromeos_data->oem_installed);
  } else {
    EXPECT_FALSE(chromeos_data.has_value());
  }

  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    EXPECT_TRUE(app_copy->HasAnySources());
    app_copy->RemoveSource(type);
  }
  EXPECT_FALSE(app_copy->HasAnySources());

  // No optional fields.
  EXPECT_EQ(app_copy->display_mode(), DisplayMode::kUndefined);
  EXPECT_TRUE(app_copy->display_mode_override().empty());
  EXPECT_TRUE(app_copy->untranslated_description().empty());
  EXPECT_TRUE(app_copy->scope().is_empty());
  EXPECT_FALSE(app_copy->theme_color().has_value());
  EXPECT_FALSE(app_copy->dark_mode_theme_color().has_value());
  EXPECT_FALSE(app_copy->background_color().has_value());
  EXPECT_FALSE(app_copy->dark_mode_background_color().has_value());
  EXPECT_TRUE(app_copy->last_badging_time().is_null());
  EXPECT_TRUE(app_copy->last_launch_time().is_null());
  EXPECT_TRUE(app_copy->first_install_time().is_null());
  EXPECT_TRUE(app_copy->manifest_icons().empty());
  EXPECT_TRUE(app_copy->downloaded_icon_sizes(IconPurpose::ANY).empty());
  EXPECT_TRUE(app_copy->downloaded_icon_sizes(IconPurpose::MASKABLE).empty());
  EXPECT_TRUE(app_copy->downloaded_icon_sizes(IconPurpose::MONOCHROME).empty());
  EXPECT_FALSE(app_copy->is_generated_icon());
  EXPECT_FALSE(app_copy->is_from_sync_and_pending_installation());
  EXPECT_FALSE(app_copy->sync_proto().has_name());
  EXPECT_FALSE(app_copy->sync_proto().has_theme_color());
  EXPECT_FALSE(app_copy->sync_proto().has_scope());
  EXPECT_EQ(app_copy->sync_proto().icon_infos_size(), 0);
  EXPECT_TRUE(app_copy->file_handlers().empty());
  EXPECT_FALSE(app_copy->share_target().has_value());
  EXPECT_TRUE(app_copy->additional_search_terms().empty());
  EXPECT_TRUE(app_copy->allowed_launch_protocols().empty());
  EXPECT_TRUE(app_copy->disallowed_launch_protocols().empty());
  EXPECT_TRUE(app_copy->url_handlers().empty());
  EXPECT_TRUE(app_copy->scope_extensions().empty());
  EXPECT_TRUE(app_copy->validated_scope_extensions().empty());
  EXPECT_TRUE(app_copy->shortcuts_menu_item_infos().empty());
  EXPECT_EQ(app_copy->run_on_os_login_mode(), RunOnOsLoginMode::kNotRun);
  EXPECT_TRUE(app_copy->manifest_url().is_empty());
  EXPECT_TRUE(app_copy->permissions_policy().empty());
  EXPECT_FALSE(app_copy->tab_strip());
  EXPECT_TRUE(app_copy->latest_install_time().is_null());
}

TEST_P(WebAppDatabaseTest, WebAppWithManyIcons) {
  InitSyncBridge();

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

TEST_P(WebAppDatabaseTest, MigrateOldLaunchHandlerSyntax) {
  std::unique_ptr<WebApp> base_app = test::CreateRandomWebApp({});
  std::unique_ptr<WebAppProto> base_proto =
      WebAppDatabase::CreateWebAppProto(*base_app);

  // "launch_handler": {
  //   "route_to": "existing-client",
  //   "navigate_existing_client": "always"
  // }
  // ->
  // "launch_handler": {
  //   "client_mode": "navigate-existing"
  // }
  WebAppProto old_navigate_proto(*base_proto);
  old_navigate_proto.mutable_launch_handler()->set_route_to(
      LaunchHandlerProto_DeprecatedRouteTo_EXISTING_CLIENT);
  old_navigate_proto.mutable_launch_handler()->set_navigate_existing_client(
      LaunchHandlerProto_DeprecatedNavigateExistingClient_ALWAYS);
  old_navigate_proto.mutable_launch_handler()->set_client_mode(
      LaunchHandlerProto_ClientMode_UNSPECIFIED_CLIENT_MODE);

  std::unique_ptr<WebApp> new_navigate_app =
      WebAppDatabase::CreateWebApp(old_navigate_proto);
  EXPECT_EQ(new_navigate_app->launch_handler(),
            (LaunchHandler{LaunchHandler::ClientMode::kNavigateExisting}))
      << new_navigate_app->launch_handler()->client_mode;

  std::unique_ptr<WebAppProto> new_navigate_proto =
      WebAppDatabase::CreateWebAppProto(*new_navigate_app);
  EXPECT_EQ(new_navigate_proto->launch_handler().route_to(),
            LaunchHandlerProto_DeprecatedRouteTo_UNSPECIFIED_ROUTE);
  EXPECT_EQ(
      new_navigate_proto->launch_handler().navigate_existing_client(),
      LaunchHandlerProto_DeprecatedNavigateExistingClient_UNSPECIFIED_NAVIGATE);
  EXPECT_EQ(new_navigate_proto->launch_handler().client_mode(),
            LaunchHandlerProto_ClientMode_NAVIGATE_EXISTING);

  // "launch_handler": {
  //   "route_to": "existing-client",
  //   "navigate_existing_client": "never"
  // }
  // ->
  // "launch_handler": {
  //   "client_mode": "focus-existing"
  // }
  WebAppProto old_focus_proto(*base_proto);
  old_focus_proto.mutable_launch_handler()->set_route_to(
      LaunchHandlerProto_DeprecatedRouteTo_EXISTING_CLIENT);
  old_focus_proto.mutable_launch_handler()->set_navigate_existing_client(
      LaunchHandlerProto_DeprecatedNavigateExistingClient_NEVER);
  old_focus_proto.mutable_launch_handler()->set_client_mode(
      LaunchHandlerProto_ClientMode_UNSPECIFIED_CLIENT_MODE);

  std::unique_ptr<WebApp> new_focus_app =
      WebAppDatabase::CreateWebApp(old_focus_proto);
  EXPECT_EQ(new_focus_app->launch_handler(),
            (LaunchHandler{LaunchHandler::ClientMode::kFocusExisting}));

  std::unique_ptr<WebAppProto> new_focus_proto =
      WebAppDatabase::CreateWebAppProto(*new_focus_app);
  EXPECT_EQ(new_focus_proto->launch_handler().route_to(),
            LaunchHandlerProto_DeprecatedRouteTo_UNSPECIFIED_ROUTE);
  EXPECT_EQ(
      new_focus_proto->launch_handler().navigate_existing_client(),
      LaunchHandlerProto_DeprecatedNavigateExistingClient_UNSPECIFIED_NAVIGATE);
  EXPECT_EQ(new_focus_proto->launch_handler().client_mode(),
            LaunchHandlerProto_ClientMode_FOCUS_EXISTING);
}

// Tests handling crashes fixed in crbug.com/1417955.
TEST_P(WebAppDatabaseTest, MigrateFromMissingShortcutsSizes) {
  std::unique_ptr<WebApp> base_app = test::CreateRandomWebApp({});
  WebAppShortcutsMenuItemInfo shortcut_item_info{};
  shortcut_item_info.name = u"shortcut";
  shortcut_item_info.url = GURL("http://example.com/shortcut");
  shortcut_item_info.downloaded_icon_sizes.any = {42};
  shortcut_item_info.downloaded_icon_sizes.maskable = {24};
  shortcut_item_info.downloaded_icon_sizes.monochrome = {123};
  base_app->SetShortcutsMenuInfo({shortcut_item_info});

  std::unique_ptr<WebAppProto> base_proto =
      WebAppDatabase::CreateWebAppProto(*base_app);

  WebAppProto proto_without_shortcut_info(*base_proto);
  proto_without_shortcut_info.clear_shortcuts_menu_item_infos();
  // Fail to parse when fewer shortcut infos than downloaded sizes. No evidence
  // this happens in the wild.
  EXPECT_EQ(WebAppDatabase::CreateWebApp(proto_without_shortcut_info), nullptr);

  // If DB is missing downloaded shortcut icon sizes information, expect to pad
  // the vector with empty IconSizes structs so the vectors in WebApp have equal
  // length.
  WebAppProto proto_without_downloaded_sizes(*base_proto);
  proto_without_downloaded_sizes.clear_downloaded_shortcuts_menu_icons_sizes();
  auto roundtrip_app =
      WebAppDatabase::CreateWebApp(proto_without_downloaded_sizes);

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
TEST_P(WebAppDatabaseTest, RemovesFragmentFromSyncProtoManifestIdPath) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<WebApp> app = test::CreateRandomWebApp({});
  // Apps must always have a valid manifest ID without a ref.
  EXPECT_TRUE(app->manifest_id().is_valid());
  EXPECT_FALSE(app->manifest_id().has_ref());
  std::string relative_manifest_id_path =
      app->sync_proto().relative_manifest_id();

  std::unique_ptr<WebAppProto> proto = WebAppDatabase::CreateWebAppProto(*app);
  proto->mutable_sync_data()->set_relative_manifest_id(
      relative_manifest_id_path + "#fragment");
  EXPECT_EQ(proto->sync_data().relative_manifest_id(),
            relative_manifest_id_path + "#fragment");

  // Re-parse the app from the proto.
  auto roundtrip_app = WebAppDatabase::CreateWebApp(*proto);
  ASSERT_TRUE(roundtrip_app);

  // Loaded app should have had the fragment stripped.
  EXPECT_EQ(roundtrip_app->sync_proto().relative_manifest_id(),
            relative_manifest_id_path);
  EXPECT_FALSE(roundtrip_app->manifest_id().has_ref());

  histogram_tester.ExpectUniqueSample("WebApp.CreateWebApp.ManifestIdMatch",
                                      false, 1);
}

TEST_P(WebAppDatabaseTest, RemovesFragmentAndQueriesFromScopeDuringParsing) {
  std::unique_ptr<WebApp> app = test::CreateRandomWebApp({});
  EXPECT_TRUE(app->scope().is_valid());
  EXPECT_FALSE(app->scope().has_ref());
  std::string basic_scope_path = app->scope().spec();
  std::string scope_path_with_queries_and_fragment =
      base::StrCat({basic_scope_path, "?query=abc", "fragment"});

  // Create a WebAppProto with a scope that has queries and fragments.
  std::unique_ptr<WebAppProto> proto = WebAppDatabase::CreateWebAppProto(*app);
  proto->set_scope(scope_path_with_queries_and_fragment);
  EXPECT_EQ(proto->scope(), scope_path_with_queries_and_fragment);

  // Re-parse the app from the proto.
  auto reparsed_app = WebAppDatabase::CreateWebApp(*proto);
  ASSERT_TRUE(reparsed_app);

  // Loaded app should have had the fragment and query stripped.
  EXPECT_EQ(reparsed_app->scope(), basic_scope_path);
  EXPECT_FALSE(reparsed_app->scope().has_ref());
  EXPECT_FALSE(reparsed_app->scope().has_query());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(WebAppDatabaseTest);

class WebAppDatabaseProtoDataTest : public ::testing::Test {
 public:
  std::unique_ptr<WebApp> CreateMinimalWebApp() {
    GURL start_url{"https://example.com/"};
    webapps::AppId app_id =
        GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->SetStartUrl(start_url);
    web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    web_app->AddSource(WebAppManagement::Type::kDefault);
    return web_app;
  }

  std::unique_ptr<WebApp> CreateIsolatedWebApp(
      const IsolationData& isolation_data) {
    std::unique_ptr<WebApp> web_app = CreateMinimalWebApp();
    web_app->SetIsolationData(isolation_data);
    return web_app;
  }

  std::unique_ptr<WebApp> CreateWebAppWithPermissionsPolicy(
      const blink::ParsedPermissionsPolicy& permissions_policy) {
    std::unique_ptr<WebApp> web_app = CreateMinimalWebApp();
    web_app->SetPermissionsPolicy(permissions_policy);
    return web_app;
  }

  std::unique_ptr<WebApp> ToAndFromProto(const WebApp& web_app) {
    return WebAppDatabase::CreateWebApp(
        *WebAppDatabase::CreateWebAppProto(web_app));
  }
};

TEST_F(WebAppDatabaseProtoDataTest, DoesNotSetIsolationDataIfNotIsolated) {
  std::unique_ptr<WebApp> web_app = CreateMinimalWebApp();
  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, AllOf(Eq(*protoed_web_app),
                              Property("isolation_data",
                                       &WebApp::isolation_data, std::nullopt)));
}

TEST_F(WebAppDatabaseProtoDataTest, SavesOwnedBundleIsolationData) {
  std::string dir_name_ascii = "folder_name";
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(
          IwaStorageOwnedBundle{dir_name_ascii, /*dev_mode=*/false},
          base::Version("1.0.0"))
          .Build());

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(web_app,
              test::IwaIs(_, IsolationData::Builder(
                                 IwaStorageOwnedBundle(dir_name_ascii,
                                                       /*dev_mode=*/false),
                                 base::Version("1.0.0"))
                                 .Build()));
}

TEST_F(WebAppDatabaseProtoDataTest, HandlesCorruptedOwnedBundleIsolationData) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"folder_name", /*dev_mode=*/false},
          base::Version("1.0.0"))
          .Build());

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  // Setting non-ASCII characters should break deserialization.
  web_app_proto->mutable_isolation_data()
      ->mutable_owned_bundle()
      ->mutable_dir_name_ascii()
      ->assign("日本");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest, SavesUnownedBundleIsolationData) {
  base::FilePath path(FILE_PATH_LITERAL("dev_bundle_path"));
  std::unique_ptr<WebApp> web_app =
      CreateIsolatedWebApp(IsolationData::Builder(IwaStorageUnownedBundle{path},
                                                  base::Version("1.0.0"))
                               .Build());

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(web_app, test::IwaIs(_, IsolationData::Builder(
                                          IwaStorageUnownedBundle{path},
                                          base::Version("1.0.0"))
                                          .Build()));
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesCorruptedUnownedBundleIsolationData) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  std::unique_ptr<WebApp> web_app =
      CreateIsolatedWebApp(IsolationData::Builder(IwaStorageUnownedBundle{path},
                                                  base::Version("1.0.0"))
                               .Build());

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  // The path is encoded with Pickle, thus setting some non-pickle data here
  // should break deserialization.
  web_app_proto->mutable_isolation_data()
      ->mutable_unowned_bundle()
      ->mutable_path()
      ->assign("foo");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest, SavesProxyIsolationData) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(IwaStorageProxy{url::Origin::Create(
                                 GURL("https://proxy-example.com/"))},
                             base::Version("1.0.0"))
          .Build());

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(web_app,
              test::IwaIs(_, IsolationData::Builder(
                                 IwaStorageProxy{url::Origin::Create(
                                     GURL("https://proxy-example.com/"))},
                                 base::Version("1.0.0"))
                                 .Build()));
}

TEST_F(WebAppDatabaseProtoDataTest, HandlesCorruptedProxyIsolationData) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(IwaStorageProxy{url::Origin::Create(
                                 GURL("https://proxy-example.com/"))},
                             base::Version("1.0.0"))
          .Build());

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  web_app_proto->mutable_isolation_data()
      ->mutable_proxy()
      ->mutable_proxy_url()
      ->assign("");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest, HandlesCorruptedIsolationDataVersion) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"folder_name", /*dev_mode=*/false},
          base::Version("1.2.3"))
          .Build());

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());
  web_app_proto->mutable_isolation_data()->mutable_version()->assign("abc");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesCorruptedIsolationDataPendingUpdateVersion) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"folder_name", /*dev_mode=*/false},
          base::Version("1.2.3"))
          .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
              IwaStorageOwnedBundle{"folder_name", /*dev_mode=*/false},
              base::Version("1.2.3")))
          .Build());

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());
  web_app_proto->mutable_isolation_data()
      ->mutable_pending_update_info()
      ->mutable_version()
      ->assign("abc");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesDifferentTypeOfIsolationDataPendingUpdateLocation) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"folder_name", /*dev_mode*/ true},
          base::Version("1.0.0"))
          .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
              IwaStorageProxy{url::Origin::Create(GURL("https://example.com"))},
              base::Version("2.0.0")))
          .Build());

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, NotNull());
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesMismatchedIsolationDataPendingUpdateLocation) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"folder_name", /*dev_mode*/ false},
          base::Version("1.0.0"))
          .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
              IwaStorageOwnedBundle{"folder_name", /*dev_mode*/ false},
              base::Version("2.0.0")))
          .Build());

  // Test what happens if both are owned bundles, but one is dev mode and
  // the other one is not.
  {
    std::unique_ptr<WebAppProto> web_app_proto =
        WebAppDatabase::CreateWebAppProto(*web_app);
    ASSERT_THAT(web_app_proto, NotNull());
    web_app_proto->mutable_isolation_data()
        ->mutable_pending_update_info()
        ->mutable_owned_bundle()
        ->set_dev_mode(true);
    web_app_proto->mutable_isolation_data()
        ->mutable_pending_update_info()
        ->mutable_proxy();

    std::unique_ptr<WebApp> protoed_web_app =
        WebAppDatabase::CreateWebApp(*web_app_proto);
    EXPECT_THAT(protoed_web_app, IsNull());
  }

  // Test what happens if one is an owned non-dev-mode bundle, but the other one
  // is a proxy.
  {
    std::unique_ptr<WebAppProto> web_app_proto =
        WebAppDatabase::CreateWebAppProto(*web_app);
    ASSERT_THAT(web_app_proto, NotNull());
    web_app_proto->mutable_isolation_data()
        ->mutable_pending_update_info()
        ->clear_location();
    web_app_proto->mutable_isolation_data()
        ->mutable_pending_update_info()
        ->mutable_proxy()
        ->set_proxy_url("https://example.com");

    std::unique_ptr<WebApp> protoed_web_app =
        WebAppDatabase::CreateWebApp(*web_app_proto);
    EXPECT_THAT(protoed_web_app, IsNull());
  }
}

TEST_F(WebAppDatabaseProtoDataTest, SavesIsolationDataUpdateInfo) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  base::FilePath update_path(FILE_PATH_LITERAL("update_path"));

  static constexpr std::string_view kUpdateManifestUrl =
      "https://update-manifest.com";

  auto integrity_block_data =
      IsolatedWebAppIntegrityBlockData(test::CreateSignatures());

  IsolationData isolation_data =
      IsolationData::Builder(IwaStorageUnownedBundle{path},
                             base::Version("1.0.0"))
          .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
              IwaStorageUnownedBundle{update_path}, base::Version("2.0.0"),
              integrity_block_data))
          .SetIntegrityBlockData(integrity_block_data)
          .SetUpdateManifestUrl(GURL(kUpdateManifestUrl))
          .Build();

  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(isolation_data);

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(web_app, test::IwaIs(_, isolation_data));
}

TEST_F(WebAppDatabaseProtoDataTest, PermissionsPolicyRoundTrip) {
  const blink::ParsedPermissionsPolicy policy = {
      {blink::mojom::PermissionsPolicyFeature::kGyroscope,
       /*allowed_origins=*/{},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/true},
      {blink::mojom::PermissionsPolicyFeature::kGeolocation,
       /*allowed_origins=*/{},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/true,
       /*matches_opaque_src=*/false},
      {blink::mojom::PermissionsPolicyFeature::kGamepad,
       {*blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.com")),
            /*has_subdomain_wildcard=*/false),
        *blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.net")),
            /*has_subdomain_wildcard=*/true)},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/false},
  };
  std::unique_ptr<WebApp> web_app = CreateWebAppWithPermissionsPolicy(policy);

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_EQ(policy, protoed_web_app->permissions_policy());
}

TEST_F(WebAppDatabaseProtoDataTest, PermissionsPolicyProto) {
  const blink::ParsedPermissionsPolicy policy = {
      {blink::mojom::PermissionsPolicyFeature::kGyroscope,
       /*allowed_origins=*/{},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/true},
      {blink::mojom::PermissionsPolicyFeature::kGeolocation,
       /*allowed_origins=*/{},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/true,
       /*matches_opaque_src=*/false},
      {blink::mojom::PermissionsPolicyFeature::kGamepad,
       {*blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.com")),
            /*has_subdomain_wildcard=*/false),
        *blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.net")),
            /*has_subdomain_wildcard=*/true)},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/false},
  };
  std::unique_ptr<WebApp> web_app = CreateWebAppWithPermissionsPolicy(policy);

  std::unique_ptr<WebAppProto> proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_EQ(proto->permissions_policy().size(), 3);
  EXPECT_EQ(proto->permissions_policy().at(0).feature(), "gyroscope");
  EXPECT_EQ(proto->permissions_policy().at(0).allowed_origins_size(), 0);
  EXPECT_EQ(proto->permissions_policy().at(0).matches_all_origins(), false);
  EXPECT_EQ(proto->permissions_policy().at(0).matches_opaque_src(), true);
  EXPECT_EQ(proto->permissions_policy().at(1).feature(), "geolocation");
  EXPECT_EQ(proto->permissions_policy().at(1).allowed_origins_size(), 0);
  EXPECT_EQ(proto->permissions_policy().at(1).matches_all_origins(), true);
  EXPECT_EQ(proto->permissions_policy().at(1).matches_opaque_src(), false);
  EXPECT_EQ(proto->permissions_policy().at(2).feature(), "gamepad");
  ASSERT_EQ(proto->permissions_policy().at(2).allowed_origins_size(), 2);
  EXPECT_EQ(proto->permissions_policy().at(2).allowed_origins(0),
            "https://example.com");
  EXPECT_EQ(proto->permissions_policy().at(2).allowed_origins(1),
            "https://*.example.net");
  EXPECT_EQ(proto->permissions_policy().at(2).matches_all_origins(), false);
  EXPECT_EQ(proto->permissions_policy().at(2).matches_opaque_src(), false);
}

}  // namespace web_app
