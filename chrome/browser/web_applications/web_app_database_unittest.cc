// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

class WebAppDatabaseTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());
  }

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(mutable_registrar().registry(), registry);
  }

  void WriteBatch(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch) {
    base::RunLoop run_loop;

    database_factory().store()->CommitWriteBatch(
        std::move(write_batch),
        base::BindLambdaForTesting(
            [&](const absl::optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Registry WriteWebApps(const GURL& base_url, int num_apps) {
    Registry registry;

    auto write_batch = database_factory().store()->CreateWriteBatch();

    for (int i = 0; i < num_apps; ++i) {
      std::unique_ptr<WebApp> app =
          test::CreateRandomWebApp(base_url, /*seed=*/i);
      std::unique_ptr<WebAppProto> proto =
          WebAppDatabase::CreateWebAppProto(*app);
      const AppId app_id = app->app_id();

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(app));
    }

    WriteBatch(std::move(write_batch));

    return registry;
  }

 protected:
  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  FakeWebAppDatabaseFactory& database_factory() {
    return controller().database_factory();
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }

  WebAppRegistrarMutable& mutable_registrar() {
    return controller().mutable_registrar();
  }

  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
};

TEST_F(WebAppDatabaseTest, WriteAndReadRegistry) {
  controller().Init();
  EXPECT_TRUE(registrar().is_empty());

  const int num_apps = 20;
  const GURL base_url("https://example.com/path");

  std::unique_ptr<WebApp> app = test::CreateRandomWebApp(base_url, /*seed=*/0);
  AppId app_id = app->app_id();
  controller().RegisterApp(std::move(app));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  for (int i = 1; i <= num_apps; ++i) {
    std::unique_ptr<WebApp> extra_app =
        test::CreateRandomWebApp(base_url, /*seed=*/i);
    controller().RegisterApp(std::move(extra_app));
  }
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  controller().UnregisterApp(app_id);
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  controller().UnregisterAll();
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppDatabaseTest, WriteAndDeleteAppsWithCallbacks) {
  controller().Init();
  EXPECT_TRUE(registrar().is_empty());

  const int num_apps = 10;
  const GURL base_url("https://example.com/path");

  RegistryUpdateData::Apps apps_to_create;
  std::vector<AppId> apps_to_delete;
  Registry expected_registry;

  for (int i = 0; i < num_apps; ++i) {
    std::unique_ptr<WebApp> app =
        test::CreateRandomWebApp(base_url, /*seed=*/i);
    apps_to_delete.push_back(app->app_id());
    apps_to_create.push_back(std::move(app));

    std::unique_ptr<WebApp> expected_app =
        test::CreateRandomWebApp(base_url, /*seed=*/i);
    expected_registry.emplace(expected_app->app_id(), std::move(expected_app));
  }

  {
    base::RunLoop run_loop;

    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

    for (std::unique_ptr<WebApp>& web_app : apps_to_create)
      update->CreateApp(std::move(web_app));

    sync_bridge().CommitUpdate(std::move(update),
                               base::BindLambdaForTesting([&](bool success) {
                                 EXPECT_TRUE(success);
                                 run_loop.Quit();
                               }));
    run_loop.Run();

    Registry registry_written = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(registry_written, expected_registry));
  }

  {
    base::RunLoop run_loop;

    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

    for (const AppId& app_id : apps_to_delete)
      update->DeleteApp(app_id);

    sync_bridge().CommitUpdate(std::move(update),
                               base::BindLambdaForTesting([&](bool success) {
                                 EXPECT_TRUE(success);
                                 run_loop.Quit();
                               }));
    run_loop.Run();

    Registry registry_deleted = database_factory().ReadRegistry();
    EXPECT_TRUE(registry_deleted.empty());
  }
}

TEST_F(WebAppDatabaseTest, OpenDatabaseAndReadRegistry) {
  Registry registry = WriteWebApps(GURL("https://example.com/path"), 20);

  controller().Init();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

TEST_F(WebAppDatabaseTest, BackwardCompatibility_WebAppWithOnlyRequiredFields) {
  const GURL start_url{"https://example.com/"};
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  const std::string name = "App Name";
  const auto user_display_mode = DisplayMode::kBrowser;
  const bool is_locally_installed = true;

  std::vector<std::unique_ptr<WebAppProto>> protos;

  // Create a proto with |required| only fields.
  // Do not add new fields in this test: any new fields should be |optional|.
  auto proto = std::make_unique<WebAppProto>();
  {
    sync_pb::WebAppSpecifics sync_proto;
    sync_proto.set_start_url(start_url.spec());
    sync_proto.set_user_display_mode(
        ToWebAppSpecificsUserDisplayMode(user_display_mode));
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
  controller().Init();

  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(start_url, app->start_url());
  EXPECT_EQ(name, app->untranslated_name());
  EXPECT_EQ(user_display_mode, app->user_display_mode());
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
  controller().Init();

  const auto start_url = GURL("https://example.com/");
  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(start_url));
  const std::string name = "Name";
  const auto user_display_mode = DisplayMode::kBrowser;

  auto app = std::make_unique<WebApp>(app_id);

  // Required fields:
  app->SetStartUrl(start_url);
  app->SetName(name);
  app->SetUserDisplayMode(user_display_mode);
  app->SetIsLocallyInstalled(false);
  // chromeos_data should always be set on ChromeOS.
  if (IsChromeOsDataMandatory())
    app->SetWebAppChromeOsData(absl::make_optional<WebAppChromeOsData>());

  EXPECT_FALSE(app->HasAnySources());
  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    app->AddSource(static_cast<WebAppManagement::Type>(i));
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
  EXPECT_TRUE(app->last_badging_time().is_null());
  EXPECT_TRUE(app->last_launch_time().is_null());
  EXPECT_TRUE(app->install_time().is_null());
  EXPECT_TRUE(app->shortcuts_menu_item_infos().empty());
  EXPECT_TRUE(app->downloaded_shortcuts_menu_icons_sizes().empty());
  EXPECT_EQ(app->run_on_os_login_mode(), RunOnOsLoginMode::kNotRun);
  EXPECT_FALSE(app->run_on_os_login_os_integration_state().has_value());
  EXPECT_TRUE(app->manifest_url().is_empty());
  EXPECT_FALSE(app->manifest_id().has_value());
  EXPECT_FALSE(app->IsStorageIsolated());
  EXPECT_TRUE(app->permissions_policy().empty());
  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);

  // Required fields were serialized:
  EXPECT_EQ(app_id, app_copy->app_id());
  EXPECT_EQ(start_url, app_copy->start_url());
  EXPECT_EQ(name, app_copy->untranslated_name());
  EXPECT_EQ(user_display_mode, app_copy->user_display_mode());
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

  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    EXPECT_TRUE(app_copy->HasAnySources());
    app_copy->RemoveSource(static_cast<WebAppManagement::Type>(i));
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
  EXPECT_TRUE(app_copy->install_time().is_null());
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
  EXPECT_TRUE(app_copy->shortcuts_menu_item_infos().empty());
  EXPECT_TRUE(app_copy->downloaded_shortcuts_menu_icons_sizes().empty());
  EXPECT_EQ(app_copy->run_on_os_login_mode(), RunOnOsLoginMode::kNotRun);
  EXPECT_FALSE(app_copy->run_on_os_login_os_integration_state().has_value());
  EXPECT_TRUE(app_copy->manifest_url().is_empty());
  EXPECT_FALSE(app_copy->manifest_id().has_value());
  EXPECT_FALSE(app_copy->IsStorageIsolated());
  EXPECT_TRUE(app_copy->permissions_policy().empty());
}

TEST_F(WebAppDatabaseTest, WebAppWithManyIcons) {
  controller().Init();

  const GURL base_url("https://example.com/path");
  // A number of icons of each IconPurpose.
  const int num_icons = 32;

  std::unique_ptr<WebApp> app = test::CreateRandomWebApp(base_url, /*seed=*/0);
  AppId app_id = app->app_id();

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

  controller().RegisterApp(std::move(app));

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
  std::unique_ptr<WebApp> base_app =
      test::CreateRandomWebApp(GURL("https://example.com"), /*seed=*/0);
  std::unique_ptr<WebAppProto> base_proto =
      WebAppDatabase::CreateWebAppProto(*base_app);

  // "launch_handler": {
  //   "route_to": "existing-client",
  //   "navigate_existing_client": "always"
  // }
  // ->
  // "launch_handler": {
  //   "route_to": "existing-client-navigate"
  // }
  WebAppProto old_navigate_proto(*base_proto);
  old_navigate_proto.mutable_launch_handler()->set_route_to(
      LaunchHandlerProto_RouteTo_DEPRECATED_EXISTING_CLIENT);
  old_navigate_proto.mutable_launch_handler()->set_navigate_existing_client(
      LaunchHandlerProto_NavigateExistingClient_ALWAYS);

  std::unique_ptr<WebApp> new_navigate_app =
      WebAppDatabase::CreateWebApp(old_navigate_proto);
  EXPECT_EQ(new_navigate_app->launch_handler(),
            (LaunchHandler{LaunchHandler::RouteTo::kExistingClientNavigate}));

  std::unique_ptr<WebAppProto> new_navigate_proto =
      WebAppDatabase::CreateWebAppProto(*new_navigate_app);
  EXPECT_EQ(new_navigate_proto->launch_handler().route_to(),
            LaunchHandlerProto_RouteTo_EXISTING_CLIENT_NAVIGATE);
  EXPECT_EQ(new_navigate_proto->launch_handler().navigate_existing_client(),
            LaunchHandlerProto_NavigateExistingClient_UNSPECIFIED_NAVIGATE);

  // "launch_handler": {
  //   "route_to": "existing-client",
  //   "navigate_existing_client": "never"
  // }
  // ->
  // "launch_handler": {
  //   "route_to": "existing-client-retain"
  // }
  WebAppProto old_retain_proto(*base_proto);
  old_retain_proto.mutable_launch_handler()->set_route_to(
      LaunchHandlerProto_RouteTo_DEPRECATED_EXISTING_CLIENT);
  old_retain_proto.mutable_launch_handler()->set_navigate_existing_client(
      LaunchHandlerProto_NavigateExistingClient_NEVER);

  std::unique_ptr<WebApp> new_retain_app =
      WebAppDatabase::CreateWebApp(old_retain_proto);
  EXPECT_EQ(new_retain_app->launch_handler(),
            (LaunchHandler{LaunchHandler::RouteTo::kExistingClientRetain}));

  std::unique_ptr<WebAppProto> new_retain_proto =
      WebAppDatabase::CreateWebAppProto(*new_retain_app);
  EXPECT_EQ(new_retain_proto->launch_handler().route_to(),
            LaunchHandlerProto_RouteTo_EXISTING_CLIENT_RETAIN);
  EXPECT_EQ(new_retain_proto->launch_handler().navigate_existing_client(),
            LaunchHandlerProto_NavigateExistingClient_UNSPECIFIED_NAVIGATE);
}

}  // namespace web_app
