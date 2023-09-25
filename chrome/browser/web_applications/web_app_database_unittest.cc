// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
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
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::VariantWith;

class WebAppDatabaseTest : public WebAppTest {
 public:
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

    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(mutable_registrar().registry(), registry);
  }

  void WriteBatch(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch) {
    base::RunLoop run_loop;

    database_factory().GetStore()->CommitWriteBatch(
        std::move(write_batch),
        base::BindLambdaForTesting(
            [&](const absl::optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Registry WriteWebApps(uint32_t num_apps) {
    Registry registry;

    auto write_batch = database_factory().GetStore()->CreateWriteBatch();

    for (uint32_t i = 0; i < num_apps; ++i) {
      std::unique_ptr<WebApp> app = test::CreateRandomWebApp({.seed = i});
      std::unique_ptr<WebAppProto> proto =
          WebAppDatabase::CreateWebAppProto(*app);
      const webapps::AppId app_id = app->app_id();

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(app));
    }

    WriteBatch(std::move(write_batch));

    return registry;
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

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
};

TEST_F(WebAppDatabaseTest, WriteAndReadRegistry) {
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

TEST_F(WebAppDatabaseTest, WriteAndDeleteAppsWithCallbacks) {
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

TEST_F(WebAppDatabaseTest, OpenDatabaseAndReadRegistry) {
  Registry registry = WriteWebApps(100);

  InitSyncBridge();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

TEST_F(WebAppDatabaseTest, BackwardCompatibility_WebAppWithOnlyRequiredFields) {
  const GURL start_url{"https://example.com/"};
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  const std::string name = "App Name";
  const bool is_locally_installed = true;

  std::vector<std::unique_ptr<WebAppProto>> protos;

  // Create a proto with |required| only fields.
  // Do not add new fields in this test: any new fields should be |optional|.
  auto proto = std::make_unique<WebAppProto>();
  {
    sync_pb::WebAppSpecifics sync_proto;
    sync_proto.set_start_url(start_url.spec());
    sync_proto.set_user_display_mode(
        ToWebAppSpecificsUserDisplayMode(DisplayMode::kBrowser));
    *(proto->mutable_sync_data()) = std::move(sync_proto);
  }

  proto->set_name(name);
  proto->set_is_locally_installed(is_locally_installed);

  proto->mutable_sources()->set_system(false);
  proto->mutable_sources()->set_policy(false);
  proto->mutable_sources()->set_web_app_store(false);
  proto->mutable_sources()->set_sync(true);
  proto->mutable_sources()->set_default_(false);

  if (IsChromeOsDataMandatory()) {
    proto->mutable_chromeos_data()->set_show_in_launcher(false);
    proto->mutable_chromeos_data()->set_show_in_search(false);
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
  EXPECT_EQ(is_locally_installed, app->is_locally_installed());
  EXPECT_TRUE(app->IsSynced());
  EXPECT_FALSE(app->IsPreinstalledApp());

  if (IsChromeOsDataMandatory()) {
    EXPECT_FALSE(app->chromeos_data()->show_in_launcher);
    EXPECT_FALSE(app->chromeos_data()->show_in_search);
    EXPECT_FALSE(app->chromeos_data()->show_in_management);
    EXPECT_TRUE(app->chromeos_data()->is_disabled);
  } else {
    EXPECT_FALSE(app->chromeos_data().has_value());
  }
}

TEST_F(WebAppDatabaseTest, WebAppWithoutOptionalFields) {
  InitSyncBridge();

  const auto start_url = GURL("https://example.com/");
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(start_url));
  const std::string name = "Name";

  auto app = std::make_unique<WebApp>(app_id);

  // Required fields:
  app->SetStartUrl(start_url);
  app->SetManifestId(GenerateManifestIdFromStartUrlOnly(start_url));
  app->SetName(name);
  app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  app->SetIsLocallyInstalled(false);
  // chromeos_data should always be set on ChromeOS.
  if (IsChromeOsDataMandatory())
    app->SetWebAppChromeOsData(absl::make_optional<WebAppChromeOsData>());

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
  EXPECT_TRUE(app->sync_fallback_data().name.empty());
  EXPECT_FALSE(app->sync_fallback_data().theme_color.has_value());
  EXPECT_FALSE(app->sync_fallback_data().scope.is_valid());
  EXPECT_TRUE(app->sync_fallback_data().icon_infos.empty());
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
  EXPECT_FALSE(app->run_on_os_login_os_integration_state().has_value());
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
  EXPECT_FALSE(app_copy->is_locally_installed());

  auto& chromeos_data = app_copy->chromeos_data();
  if (IsChromeOsDataMandatory()) {
    EXPECT_TRUE(chromeos_data->show_in_launcher);
    EXPECT_TRUE(chromeos_data->show_in_search);
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
  EXPECT_TRUE(app_copy->sync_fallback_data().name.empty());
  EXPECT_FALSE(app_copy->sync_fallback_data().theme_color.has_value());
  EXPECT_FALSE(app_copy->sync_fallback_data().scope.is_valid());
  EXPECT_TRUE(app_copy->sync_fallback_data().icon_infos.empty());
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
  EXPECT_FALSE(app_copy->run_on_os_login_os_integration_state().has_value());
  EXPECT_TRUE(app_copy->manifest_url().is_empty());
  EXPECT_TRUE(app_copy->permissions_policy().empty());
  EXPECT_FALSE(app_copy->tab_strip());
  EXPECT_TRUE(app_copy->latest_install_time().is_null());
}

TEST_F(WebAppDatabaseTest, WebAppWithManyIcons) {
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

TEST_F(WebAppDatabaseTest, MigrateOldLaunchHandlerSyntax) {
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
TEST_F(WebAppDatabaseTest, MigrateFromMissingShortcutsSizes) {
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

class WebAppDatabaseProtoDataTest : public ::testing::Test {
 public:
  std::unique_ptr<WebApp> CreateMinimalWebApp() {
    GURL start_url{"https://example.com/"};
    webapps::AppId app_id =
        GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->SetStartUrl(start_url);
    web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    web_app->AddSource(WebAppManagement::Type::kDefault);
    return web_app;
  }

  std::unique_ptr<WebApp> CreateIsolatedWebApp(
      const WebApp::IsolationData& isolation_data) {
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
  EXPECT_THAT(*web_app,
              AllOf(Eq(*protoed_web_app),
                    Property("isolation_data", &WebApp::isolation_data,
                             absl::nullopt)));
}

TEST_F(WebAppDatabaseProtoDataTest, SavesInstalledBundleIsolationData) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      InstalledBundle{.path = path}, base::Version("1.0.0")));

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(web_app->isolation_data()->location,
              VariantWith<InstalledBundle>(
                  Field("path", &InstalledBundle::path, Eq(path))));
  EXPECT_THAT(web_app->isolation_data()->version, Eq(base::Version("1.0.0")));
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesCorruptedInstalledBundleIsolationData) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      InstalledBundle{.path = path}, base::Version("1.0.0")));

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  // The path is encoded with Pickle, thus setting some non-pickle data here
  // should break deserialization.
  web_app_proto->mutable_isolation_data()
      ->mutable_installed_bundle()
      ->mutable_path()
      ->assign("foo");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest, SavesDevModeBundleIsolationData) {
  base::FilePath path(FILE_PATH_LITERAL("dev_bundle_path"));
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      DevModeBundle{.path = path}, base::Version("1.0.0")));

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(web_app->isolation_data()->location,
              VariantWith<DevModeBundle>(
                  Field("path", &DevModeBundle::path, Eq(path))));
  EXPECT_THAT(web_app->isolation_data()->version, Eq(base::Version("1.0.0")));
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesCorruptedDevModeBundleIsolationData) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      DevModeBundle{.path = path}, base::Version("1.0.0")));

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  // The path is encoded with Pickle, thus setting some non-pickle data here
  // should break deserialization.
  web_app_proto->mutable_isolation_data()
      ->mutable_dev_mode_bundle()
      ->mutable_path()
      ->assign("foo");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest, SavesDevModeProxyIsolationData) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      DevModeProxy{.proxy_url =
                       url::Origin::Create(GURL("https://proxy-example.com/"))},
      base::Version("1.0.0")));

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(
      web_app->isolation_data()->location,
      VariantWith<DevModeProxy>(
          Field("proxy_url", &DevModeProxy::proxy_url,
                Eq(url::Origin::Create(GURL("https://proxy-example.com/"))))));
  EXPECT_THAT(web_app->isolation_data()->version, Eq(base::Version("1.0.0")));
}

TEST_F(WebAppDatabaseProtoDataTest, HandlesCorruptedDevModeProxyIsolationData) {
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      DevModeProxy{.proxy_url =
                       url::Origin::Create(GURL("https://example.com"))},
      base::Version("1.0.0")));

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  web_app_proto->mutable_isolation_data()
      ->mutable_dev_mode_proxy()
      ->mutable_proxy_url()
      ->assign("");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest, HandlesCorruptedIsolationDataVersion) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  // The version must have three numeric parts, thus using five parts here
  // should break deserialization.
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      InstalledBundle{.path = path}, base::Version("1.2.3.4.5")));

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesCorruptedIsolationDataPendingUpdateVersion) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  // The version must have three numeric parts, thus using five parts here
  // should break deserialization.
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      InstalledBundle{.path = path}, base::Version("1.2.3"), {},
      WebApp::IsolationData::PendingUpdateInfo(InstalledBundle{.path = path},
                                               base::Version("1.2.3.4.5"))));

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest,
       HandlesMismatchedIsolationDataPendingUpdateLocation) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      InstalledBundle{.path = path}, base::Version("1.0.0"), {},
      WebApp::IsolationData::PendingUpdateInfo(InstalledBundle{.path = path},
                                               base::Version("2.0.0"))));

  std::unique_ptr<WebAppProto> web_app_proto =
      WebAppDatabase::CreateWebAppProto(*web_app);
  ASSERT_THAT(web_app_proto, NotNull());
  web_app_proto->mutable_isolation_data()
      ->mutable_pending_update_info()
      ->clear_location();
  web_app_proto->mutable_isolation_data()
      ->mutable_pending_update_info()
      ->mutable_dev_mode_proxy()
      ->set_proxy_url("https://example.com");

  std::unique_ptr<WebApp> protoed_web_app =
      WebAppDatabase::CreateWebApp(*web_app_proto);
  EXPECT_THAT(protoed_web_app, IsNull());
}

TEST_F(WebAppDatabaseProtoDataTest, SavesIsolationDataUpdateInfo) {
  base::FilePath path(FILE_PATH_LITERAL("bundle_path"));
  base::FilePath update_path(FILE_PATH_LITERAL("update_path"));
  std::unique_ptr<WebApp> web_app = CreateIsolatedWebApp(WebApp::IsolationData(
      InstalledBundle{.path = path}, base::Version("1.0.0"), {},
      WebApp::IsolationData::PendingUpdateInfo(
          InstalledBundle{.path = update_path}, base::Version("2.0.0"))));

  std::unique_ptr<WebApp> protoed_web_app = ToAndFromProto(*web_app);
  EXPECT_THAT(*web_app, Eq(*protoed_web_app));
  EXPECT_THAT(web_app->isolation_data()->location,
              VariantWith<InstalledBundle>(
                  Field("path", &InstalledBundle::path, Eq(path))));
  EXPECT_THAT(web_app->isolation_data()->version, Eq(base::Version("1.0.0")));
  EXPECT_THAT(web_app->isolation_data()->pending_update_info()->location,
              VariantWith<InstalledBundle>(
                  Field("path", &InstalledBundle::path, Eq(update_path))));
  EXPECT_THAT(web_app->isolation_data()->pending_update_info()->version,
              Eq(base::Version("2.0.0")));
}

TEST_F(WebAppDatabaseProtoDataTest, PermissionsPolicyRoundTrip) {
  const blink::ParsedPermissionsPolicy policy = {
      {blink::mojom::PermissionsPolicyFeature::kGyroscope,
       /*allowed_origins=*/{},
       /*self_if_matches=*/absl::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/true},
      {blink::mojom::PermissionsPolicyFeature::kGeolocation,
       /*allowed_origins=*/{},
       /*self_if_matches=*/absl::nullopt,
       /*matches_all_origins=*/true,
       /*matches_opaque_src=*/false},
      {blink::mojom::PermissionsPolicyFeature::kGamepad,
       {*blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.com")),
            /*has_subdomain_wildcard=*/false),
        *blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.net")),
            /*has_subdomain_wildcard=*/true)},
       /*self_if_matches=*/absl::nullopt,
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
       /*self_if_matches=*/absl::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/true},
      {blink::mojom::PermissionsPolicyFeature::kGeolocation,
       /*allowed_origins=*/{},
       /*self_if_matches=*/absl::nullopt,
       /*matches_all_origins=*/true,
       /*matches_opaque_src=*/false},
      {blink::mojom::PermissionsPolicyFeature::kGamepad,
       {*blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.com")),
            /*has_subdomain_wildcard=*/false),
        *blink::OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
            url::Origin::Create(GURL("https://example.net")),
            /*has_subdomain_wildcard=*/true)},
       /*self_if_matches=*/absl::nullopt,
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
