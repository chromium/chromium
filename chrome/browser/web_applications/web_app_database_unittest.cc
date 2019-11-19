// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/sync/model/model_type_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

class WebAppDatabaseTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());
  }

  static std::unique_ptr<WebApp> CreateWebApp(const std::string& base_url,
                                              int suffix) {
    const auto launch_url = base_url + base::NumberToString(suffix);
    const AppId app_id = GenerateAppIdFromURL(GURL(launch_url));
    const std::string name = "Name" + base::NumberToString(suffix);
    const std::string description =
        "Description" + base::NumberToString(suffix);
    const std::string scope =
        base_url + "/scope" + base::NumberToString(suffix);
    const base::Optional<SkColor> theme_color = suffix;
    const base::Optional<SkColor> synced_theme_color = suffix ^ UINT_MAX;

    auto app = std::make_unique<WebApp>(app_id);

    // Generate all possible permutations of field values in a random way:
    if (suffix & 1)
      app->AddSource(Source::kSystem);
    if (suffix & 2)
      app->AddSource(Source::kPolicy);
    if (suffix & 4)
      app->AddSource(Source::kWebAppStore);
    if (suffix & 8)
      app->AddSource(Source::kSync);
    if (suffix & 16)
      app->AddSource(Source::kDefault);
    if ((suffix & 31) == 0)
      app->AddSource(Source::kSync);

    app->SetName(name);
    app->SetDescription(description);
    app->SetLaunchUrl(GURL(launch_url));
    app->SetScope(GURL(scope));
    app->SetThemeColor(theme_color);
    app->SetIsLocallyInstalled(!(suffix & 2));
    app->SetIsInSyncInstall(!(suffix & 4));
    app->SetUserDisplayMode((suffix & 1) ? DisplayMode::kBrowser
                                         : DisplayMode::kStandalone);

    const DisplayMode display_modes[4] = {
        DisplayMode::kBrowser, DisplayMode::kMinimalUi,
        DisplayMode::kStandalone, DisplayMode::kFullscreen};
    app->SetDisplayMode(display_modes[(suffix >> 4) & 3]);

    const std::string icon_url =
        base_url + "/icon" + base::NumberToString(suffix);
    const int icon_size_in_px = 256;

    WebApp::Icons icons;
    icons.push_back({GURL(icon_url), icon_size_in_px});

    app->SetIcons(std::move(icons));

    WebApp::SyncData sync_data;
    sync_data.name = "Sync" + name;
    sync_data.theme_color = synced_theme_color;
    app->SetSyncData(std::move(sync_data));

    return app;
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
            [&](const base::Optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Registry WriteWebApps(const std::string& base_url, int num_apps) {
    Registry registry;

    auto write_batch = database_factory().store()->CreateWriteBatch();

    for (int i = 0; i < num_apps; ++i) {
      auto app = CreateWebApp(base_url, i);
      auto proto = WebAppDatabase::CreateWebAppProto(*app);
      const auto app_id = app->app_id();

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(app));
    }

    WriteBatch(std::move(write_batch));

    return registry;
  }

 protected:
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  TestWebAppDatabaseFactory& database_factory() {
    return controller().database_factory();
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }

  WebAppRegistrarMutable& mutable_registrar() {
    return controller().mutable_registrar();
  }

  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
};

TEST_F(WebAppDatabaseTest, WriteAndReadRegistry) {
  controller().Init();
  EXPECT_TRUE(registrar().is_empty());

  const int num_apps = 100;
  const std::string base_url = "https://example.com/path";

  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();
  controller().RegisterApp(std::move(app));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  for (int i = 1; i <= num_apps; ++i) {
    auto extra_app = CreateWebApp(base_url, i);
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
  const std::string base_url = "https://example.com/path";

  RegistryUpdateData::Apps apps_to_create;
  std::vector<AppId> apps_to_delete;
  Registry expected_registry;

  for (int i = 0; i < num_apps; ++i) {
    std::unique_ptr<WebApp> app = CreateWebApp(base_url, i);
    apps_to_delete.push_back(app->app_id());
    apps_to_create.push_back(std::move(app));

    std::unique_ptr<WebApp> expected_app = CreateWebApp(base_url, i);
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
  Registry registry = WriteWebApps("https://example.com/path", 100);

  controller().Init();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

TEST_F(WebAppDatabaseTest, WebAppWithoutOptionalFields) {
  controller().Init();

  const auto launch_url = GURL("https://example.com/");
  const AppId app_id = GenerateAppIdFromURL(GURL(launch_url));
  const std::string name = "Name";
  const auto user_display_mode = DisplayMode::kBrowser;

  auto app = std::make_unique<WebApp>(app_id);

  // Required fields:
  app->SetLaunchUrl(launch_url);
  app->SetName(name);
  app->SetUserDisplayMode(user_display_mode);
  app->SetIsLocallyInstalled(false);

  EXPECT_FALSE(app->HasAnySources());
  for (int i = Source::kMinValue; i < Source::kMaxValue; ++i) {
    app->AddSource(static_cast<Source::Type>(i));
    EXPECT_TRUE(app->HasAnySources());
  }

  // Let optional fields be empty:
  EXPECT_EQ(app->display_mode(), DisplayMode::kUndefined);
  EXPECT_TRUE(app->description().empty());
  EXPECT_TRUE(app->scope().is_empty());
  EXPECT_FALSE(app->theme_color().has_value());
  EXPECT_TRUE(app->icons().empty());
  EXPECT_FALSE(app->is_in_sync_install());
  EXPECT_TRUE(app->sync_data().name.empty());
  EXPECT_FALSE(app->sync_data().theme_color.has_value());
  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);

  // Required fields were serialized:
  EXPECT_EQ(app_id, app_copy->app_id());
  EXPECT_EQ(launch_url, app_copy->launch_url());
  EXPECT_EQ(name, app_copy->name());
  EXPECT_EQ(user_display_mode, app_copy->user_display_mode());
  EXPECT_FALSE(app_copy->is_locally_installed());

  for (int i = Source::kMinValue; i < Source::kMaxValue; ++i) {
    EXPECT_TRUE(app_copy->HasAnySources());
    app_copy->RemoveSource(static_cast<Source::Type>(i));
  }
  EXPECT_FALSE(app_copy->HasAnySources());

  // No optional fields.
  EXPECT_EQ(app_copy->display_mode(), DisplayMode::kUndefined);
  EXPECT_TRUE(app_copy->description().empty());
  EXPECT_TRUE(app_copy->scope().is_empty());
  EXPECT_FALSE(app_copy->theme_color().has_value());
  EXPECT_TRUE(app_copy->icons().empty());
  EXPECT_FALSE(app_copy->is_in_sync_install());
  EXPECT_TRUE(app_copy->sync_data().name.empty());
  EXPECT_FALSE(app_copy->sync_data().theme_color.has_value());
}

TEST_F(WebAppDatabaseTest, WebAppWithManyIcons) {
  controller().Init();

  const int num_icons = 32;
  const std::string base_url = "https://example.com/path";

  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();

  WebApp::Icons icons;
  for (int i = 1; i <= num_icons; ++i) {
    const std::string icon_url =
        base_url + "/icon" + base::NumberToString(num_icons);
    // Let size equals the icon's number squared.
    const int icon_size_in_px = i * i;
    icons.push_back({GURL(icon_url), icon_size_in_px});
  }
  app->SetIcons(std::move(icons));

  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);
  EXPECT_EQ(static_cast<unsigned>(num_icons), app_copy->icons().size());
  for (int i = 1; i <= num_icons; ++i) {
    const int icon_size_in_px = i * i;
    EXPECT_EQ(icon_size_in_px, app_copy->icons()[i - 1].size_in_px);
  }
}

}  // namespace web_app
