// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

Registry CreateRegistryForTesting(const std::string& base_url, int num_apps) {
  Registry registry;

  for (int i = 0; i < num_apps; ++i) {
    const auto url = base_url + base::NumberToString(i);
    const AppId app_id = GenerateAppIdFromURL(GURL(url));

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(Source::kSync);
    web_app->SetStartUrl(GURL(url));
    web_app->SetName("Name" + base::NumberToString(i));
    web_app->SetDisplayMode(DisplayMode::kBrowser);
    web_app->SetUserDisplayMode(DisplayMode::kBrowser);

    registry.emplace(app_id, std::move(web_app));
  }

  return registry;
}

}  // namespace

class WebAppRegistrarTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());
  }

 protected:
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  TestWebAppDatabaseFactory& database_factory() {
    return controller().database_factory();
  }
  WebAppRegistrar& registrar() { return controller().registrar(); }
  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }

  std::set<AppId> RegisterAppsForTesting(Registry registry) {
    std::set<AppId> ids;

    ScopedRegistryUpdate update(&sync_bridge());
    for (auto& kv : registry) {
      ids.insert(kv.first);
      update->CreateApp(std::move(kv.second));
    }

    return ids;
  }

  void RegisterApp(std::unique_ptr<WebApp> web_app) {
    controller().RegisterApp(std::move(web_app));
  }

  void UnregisterApp(const AppId& app_id) {
    controller().UnregisterApp(app_id);
  }

  void UnregisterAll() { controller().UnregisterAll(); }

  AppId InitRegistrarWithApp(std::unique_ptr<WebApp> app) {
    DCHECK(registrar().is_empty());

    const AppId app_id = app->app_id();

    Registry registry;
    registry.emplace(app_id, std::move(app));

    InitRegistrarWithRegistry(registry);
    return app_id;
  }

  std::set<AppId> InitRegistrarWithApps(const std::string& base_url,
                                        int num_apps) {
    DCHECK(registrar().is_empty());

    Registry registry = CreateRegistryForTesting(base_url, num_apps);
    return InitRegistrarWithRegistry(registry);
  }

  std::set<AppId> InitRegistrarWithRegistry(const Registry& registry) {
    std::set<AppId> app_ids;
    for (auto& kv : registry)
      app_ids.insert(kv.second->app_id());

    database_factory().WriteRegistry(registry);
    controller().Init();

    return app_ids;
  }

  std::unique_ptr<WebApp> CreateWebAppWithSource(const std::string& url,
                                                 Source::Type source) {
    const GURL start_url(url);
    const AppId app_id = GenerateAppIdFromURL(start_url);

    auto web_app = std::make_unique<WebApp>(app_id);

    web_app->AddSource(source);
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    web_app->SetName("Name");
    web_app->SetStartUrl(start_url);
    return web_app;
  }

  std::unique_ptr<WebApp> CreateWebApp(const std::string& url) {
    return CreateWebAppWithSource(url, Source::kSync);
  }

  void SyncBridgeCommitUpdate(std::unique_ptr<WebAppRegistryUpdate> update) {
    base::RunLoop run_loop;
    sync_bridge().CommitUpdate(std::move(update),
                               base::BindLambdaForTesting([&](bool success) {
                                 EXPECT_TRUE(success);
                                 run_loop.Quit();
                               }));

    run_loop.Run();
  }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
};

class WebAppRegistrarTest_DisplayOverride : public WebAppRegistrarTest {
 public:
  WebAppRegistrarTest_DisplayOverride() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebAppManifestDisplayOverride);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebAppRegistrarTest, CreateRegisterUnregister) {
  controller().Init();

  EXPECT_EQ(nullptr, registrar().GetAppById(AppId()));
  EXPECT_FALSE(registrar().GetAppById(AppId()));

  const GURL start_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(start_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;

  const GURL start_url2 = GURL("https://example.com/path2");
  const AppId app_id2 = GenerateAppIdFromURL(start_url2);

  auto web_app = std::make_unique<WebApp>(app_id);
  auto web_app2 = std::make_unique<WebApp>(app_id2);

  web_app->AddSource(Source::kSync);
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetStartUrl(start_url);
  web_app->SetScope(scope);
  web_app->SetThemeColor(theme_color);

  web_app2->AddSource(Source::kDefault);
  web_app2->SetDisplayMode(DisplayMode::kBrowser);
  web_app2->SetUserDisplayMode(DisplayMode::kBrowser);
  web_app2->SetStartUrl(start_url2);
  web_app2->SetName(name);

  EXPECT_EQ(nullptr, registrar().GetAppById(app_id));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_TRUE(registrar().is_empty());

  RegisterApp(std::move(web_app));
  EXPECT_TRUE(registrar().IsInstalled(app_id));
  const WebApp* app = registrar().GetAppById(app_id);

  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(name, app->name());
  EXPECT_EQ(description, app->description());
  EXPECT_EQ(start_url, app->start_url());
  EXPECT_EQ(scope, app->scope());
  EXPECT_EQ(theme_color, app->theme_color());

  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_FALSE(registrar().is_empty());

  RegisterApp(std::move(web_app2));
  EXPECT_TRUE(registrar().IsInstalled(app_id2));
  const WebApp* app2 = registrar().GetAppById(app_id2);
  EXPECT_EQ(app_id2, app2->app_id());
  EXPECT_FALSE(registrar().is_empty());

  UnregisterApp(app_id);
  EXPECT_FALSE(registrar().IsInstalled(app_id));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id));
  EXPECT_FALSE(registrar().is_empty());

  // Check that app2 is still registered.
  app2 = registrar().GetAppById(app_id2);
  EXPECT_TRUE(registrar().IsInstalled(app_id2));
  EXPECT_EQ(app_id2, app2->app_id());

  UnregisterApp(app_id2);
  EXPECT_FALSE(registrar().IsInstalled(app_id2));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, DestroyRegistrarOwningRegisteredApps) {
  controller().Init();

  auto web_app = CreateWebApp("https://example.com/path");
  RegisterApp(std::move(web_app));

  auto web_app2 = CreateWebApp("https://example.com/path2");
  RegisterApp(std::move(web_app2));

  controller().DestroySubsystems();
}

TEST_F(WebAppRegistrarTest, InitRegistrarAndDoForEachApp) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 100);

  for (const WebApp& web_app : registrar().AllApps()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, AllAppsMutable) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);

  for (WebApp& web_app : controller().mutable_registrar().AllAppsMutable()) {
    web_app.SetDisplayMode(DisplayMode::kStandalone);
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, DoForEachAndUnregisterAllApps) {
  controller().Init();

  Registry registry = CreateRegistryForTesting("https://example.com/path", 100);
  auto ids = RegisterAppsForTesting(std::move(registry));
  EXPECT_EQ(100UL, ids.size());

  for (const WebApp& web_app : registrar().AllApps()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());

  EXPECT_FALSE(registrar().is_empty());
  UnregisterAll();
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, WebAppSyncBridge) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 100);

  // Add 1 app after Init.
  auto web_app = CreateWebApp("https://example.com/path");
  const AppId app_id = web_app->app_id();

  RegisterApp(std::move(web_app));

  EXPECT_EQ(101UL, database_factory().ReadAllAppIds().size());
  EXPECT_EQ(101UL, controller().mutable_registrar().registry().size());

  // Remove 1 app after Init.
  UnregisterApp(app_id);
  EXPECT_EQ(100UL, controller().mutable_registrar().registry().size());
  EXPECT_EQ(100UL, database_factory().ReadAllAppIds().size());

  // Remove 100 apps after Init.
  UnregisterAll();
  EXPECT_TRUE(database_factory().ReadAllAppIds().empty());
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, GetAppDataFields) {
  controller().Init();

  const GURL start_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppIdFromURL(start_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;
  const auto display_mode = DisplayMode::kMinimalUi;
  const auto user_display_mode = DisplayMode::kStandalone;
  std::vector<DisplayMode> display_mode_override;

  EXPECT_EQ(std::string(), registrar().GetAppShortName(app_id));
  EXPECT_EQ(GURL(), registrar().GetAppStartUrl(app_id));

  auto web_app = std::make_unique<WebApp>(app_id);
  WebApp* web_app_ptr = web_app.get();

  display_mode_override.push_back(DisplayMode::kMinimalUi);
  display_mode_override.push_back(DisplayMode::kStandalone);

  web_app->AddSource(Source::kSync);
  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetThemeColor(theme_color);
  web_app->SetStartUrl(start_url);
  web_app->SetDisplayMode(display_mode);
  web_app->SetUserDisplayMode(user_display_mode);
  web_app->SetDisplayModeOverride(display_mode_override);
  web_app->SetIsLocallyInstalled(/*is_locally_installed*/ false);

  RegisterApp(std::move(web_app));

  EXPECT_EQ(name, registrar().GetAppShortName(app_id));
  EXPECT_EQ(description, registrar().GetAppDescription(app_id));
  EXPECT_EQ(theme_color, registrar().GetAppThemeColor(app_id));
  EXPECT_EQ(start_url, registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppUserDisplayMode(app_id));

  {
    std::vector<DisplayMode> app_display_mode_override =
        registrar().GetAppDisplayModeOverride(app_id);
    ASSERT_EQ(2u, app_display_mode_override.size());
    EXPECT_EQ(DisplayMode::kMinimalUi, app_display_mode_override[0]);
    EXPECT_EQ(DisplayMode::kStandalone, app_display_mode_override[1]);
  }

  {
    EXPECT_FALSE(registrar().IsLocallyInstalled(app_id));

    EXPECT_FALSE(registrar().IsLocallyInstalled("unknown"));
    web_app_ptr->SetIsLocallyInstalled(/*is_locally_installed*/ true);
    EXPECT_TRUE(registrar().IsLocallyInstalled(app_id));
  }

  {
    EXPECT_EQ(DisplayMode::kUndefined,
              registrar().GetAppUserDisplayMode("unknown"));

    web_app_ptr->SetUserDisplayMode(DisplayMode::kBrowser);
    EXPECT_EQ(DisplayMode::kBrowser, registrar().GetAppUserDisplayMode(app_id));

    sync_bridge().SetAppUserDisplayMode(app_id, DisplayMode::kStandalone,
                                        /*is_user_action=*/false);
    EXPECT_EQ(DisplayMode::kStandalone, web_app_ptr->user_display_mode());
    EXPECT_EQ(DisplayMode::kMinimalUi, web_app_ptr->display_mode());

    ASSERT_EQ(2u, web_app_ptr->display_mode_override().size());
    EXPECT_EQ(DisplayMode::kMinimalUi, web_app_ptr->display_mode_override()[0]);
    EXPECT_EQ(DisplayMode::kStandalone,
              web_app_ptr->display_mode_override()[1]);
  }
}

TEST_F(WebAppRegistrarTest, CanFindAppsInScope) {
  controller().Init();

  const GURL origin_scope("https://example.com/");

  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");

  const AppId app1_id = GenerateAppIdFromURL(app1_scope);
  const AppId app2_id = GenerateAppIdFromURL(app2_scope);
  const AppId app3_id = GenerateAppIdFromURL(app3_scope);

  std::vector<AppId> in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_EQ(0u, in_scope.size());
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(origin_scope));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(app3_scope));

  auto app1 = CreateWebApp(app1_scope.spec());
  app1->SetScope(app1_scope);
  RegisterApp(std::move(app1));

  in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(origin_scope));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(app3_scope));

  in_scope = registrar().FindAppsInScope(app1_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(app1_scope));

  auto app2 = CreateWebApp(app2_scope.spec());
  app2->SetScope(app2_scope);
  RegisterApp(std::move(app2));

  in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id, app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(origin_scope));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(app3_scope));

  in_scope = registrar().FindAppsInScope(app1_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id, app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(app1_scope));

  in_scope = registrar().FindAppsInScope(app2_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(app2_scope));

  auto app3 = CreateWebApp(app3_scope.spec());
  app3->SetScope(app3_scope);
  RegisterApp(std::move(app3));

  in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id, app2_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(origin_scope));

  in_scope = registrar().FindAppsInScope(app3_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app3_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(app3_scope));
}

TEST_F(WebAppRegistrarTest, CanFindAppWithUrlInScope) {
  controller().Init();

  const GURL origin_scope("https://example.com/");

  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");

  const AppId app1_id = GenerateAppIdFromURL(app1_scope);
  const AppId app2_id = GenerateAppIdFromURL(app2_scope);
  const AppId app3_id = GenerateAppIdFromURL(app3_scope);

  auto app1 = CreateWebApp(app1_scope.spec());
  app1->SetScope(app1_scope);
  RegisterApp(std::move(app1));

  base::Optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_scope);
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, app1_id);

  base::Optional<AppId> app3_match =
      registrar().FindAppWithUrlInScope(app3_scope);
  EXPECT_FALSE(app3_match);

  auto app2 = CreateWebApp(app2_scope.spec());
  app2->SetScope(app2_scope);
  RegisterApp(std::move(app2));

  auto app3 = CreateWebApp(app3_scope.spec());
  app3->SetScope(app3_scope);
  RegisterApp(std::move(app3));

  base::Optional<AppId> origin_match =
      registrar().FindAppWithUrlInScope(origin_scope);
  EXPECT_FALSE(origin_match);

  base::Optional<AppId> app1_match =
      registrar().FindAppWithUrlInScope(app1_scope);
  DCHECK(app1_match);
  EXPECT_EQ(*app1_match, app1_id);

  app2_match = registrar().FindAppWithUrlInScope(app2_scope);
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, app2_id);

  app3_match = registrar().FindAppWithUrlInScope(app3_scope);
  DCHECK(app3_match);
  EXPECT_EQ(*app3_match, app3_id);
}

TEST_F(WebAppRegistrarTest, CanFindShortcutWithUrlInScope) {
  controller().Init();

  const GURL app1_page("https://example.com/app/page");
  const GURL app2_page("https://example.com/app-two/page");
  const GURL app3_page("https://not-example.com/app/page");

  const GURL app1_launch("https://example.com/app/launch");
  const GURL app2_launch("https://example.com/app-two/launch");
  const GURL app3_launch("https://not-example.com/app/launch");

  const AppId app1_id = GenerateAppIdFromURL(app1_launch);
  const AppId app2_id = GenerateAppIdFromURL(app2_launch);
  const AppId app3_id = GenerateAppIdFromURL(app3_launch);

  // Implicit scope "https://example.com/app/"
  auto app1 = CreateWebApp(app1_launch.spec());
  RegisterApp(std::move(app1));

  base::Optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_page);
  EXPECT_FALSE(app2_match);

  base::Optional<AppId> app3_match =
      registrar().FindAppWithUrlInScope(app3_page);
  EXPECT_FALSE(app3_match);

  auto app2 = CreateWebApp(app2_launch.spec());
  RegisterApp(std::move(app2));

  auto app3 = CreateWebApp(app3_launch.spec());
  RegisterApp(std::move(app3));

  base::Optional<AppId> app1_match =
      registrar().FindAppWithUrlInScope(app1_page);
  DCHECK(app1_match);
  EXPECT_EQ(app1_match, base::Optional<AppId>(app1_id));

  app2_match = registrar().FindAppWithUrlInScope(app2_page);
  DCHECK(app2_match);
  EXPECT_EQ(app2_match, base::Optional<AppId>(app2_id));

  app3_match = registrar().FindAppWithUrlInScope(app3_page);
  DCHECK(app3_match);
  EXPECT_EQ(app3_match, base::Optional<AppId>(app3_id));
}

TEST_F(WebAppRegistrarTest, FindPwaOverShortcut) {
  controller().Init();

  const GURL app1_launch("https://example.com/app/specific/launch1");

  const GURL app2_scope("https://example.com/app");
  const GURL app2_page("https://example.com/app/specific/page2");
  const AppId app2_id = GenerateAppIdFromURL(app2_scope);

  const GURL app3_launch("https://example.com/app/specific/launch3");

  auto app1 = CreateWebApp(app1_launch.spec());
  RegisterApp(std::move(app1));

  auto app2 = CreateWebApp(app2_scope.spec());
  app2->SetScope(app2_scope);
  RegisterApp(std::move(app2));

  auto app3 = CreateWebApp(app3_launch.spec());
  RegisterApp(std::move(app3));

  base::Optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_page);
  DCHECK(app2_match);
  EXPECT_EQ(app2_match, base::Optional<AppId>(app2_id));
}

TEST_F(WebAppRegistrarTest, BeginAndCommitUpdate) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);

  std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

  for (auto& app_id : ids) {
    WebApp* app = update->UpdateApp(app_id);
    EXPECT_TRUE(app);
    app->SetName("New Name");
  }

  // Acquire each app second time to make sure update requests get merged.
  for (auto& app_id : ids) {
    WebApp* app = update->UpdateApp(app_id);
    EXPECT_TRUE(app);
    app->SetDisplayMode(DisplayMode::kStandalone);
  }

  SyncBridgeCommitUpdate(std::move(update));

  // Make sure that all app ids were written to the database.
  auto registry_written = database_factory().ReadRegistry();
  EXPECT_EQ(ids.size(), registry_written.size());

  for (auto& kv : registry_written) {
    EXPECT_EQ("New Name", kv.second->name());
    ids.erase(kv.second->app_id());
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, CommitEmptyUpdate) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);
  const auto initial_registry = database_factory().ReadRegistry();

  {
    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();
    SyncBridgeCommitUpdate(std::move(update));

    auto registry = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(initial_registry, registry));
  }

  {
    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();
    update.reset();
    SyncBridgeCommitUpdate(std::move(update));

    auto registry = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(initial_registry, registry));
  }

  {
    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

    WebApp* app = update->UpdateApp("unknown");
    EXPECT_FALSE(app);

    SyncBridgeCommitUpdate(std::move(update));

    auto registry = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(initial_registry, registry));
  }
}

TEST_F(WebAppRegistrarTest, ScopedRegistryUpdate) {
  std::set<AppId> ids = InitRegistrarWithApps("https://example.com/path", 10);
  const auto initial_registry = database_factory().ReadRegistry();

  // Test empty update first.
  { ScopedRegistryUpdate update(&sync_bridge()); }
  EXPECT_TRUE(
      IsRegistryEqual(initial_registry, database_factory().ReadRegistry()));

  {
    ScopedRegistryUpdate update(&sync_bridge());

    for (auto& app_id : ids) {
      WebApp* app = update->UpdateApp(app_id);
      EXPECT_TRUE(app);
      app->SetDescription("New Description");
    }
  }

  // Make sure that all app ids were written to the database.
  auto updated_registry = database_factory().ReadRegistry();
  EXPECT_EQ(ids.size(), updated_registry.size());

  for (auto& kv : updated_registry) {
    EXPECT_EQ(kv.second->description(), "New Description");
    ids.erase(kv.second->app_id());
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, CopyOnWrite) {
  controller().Init();

  const GURL start_url("https://example.com");
  const AppId app_id = GenerateAppIdFromURL(start_url);
  const WebApp* app = nullptr;
  {
    auto new_app = CreateWebApp(start_url.spec());
    app = new_app.get();
    RegisterApp(std::move(new_app));
  }

  {
    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

    WebApp* app_copy = update->UpdateApp(app_id);
    EXPECT_TRUE(app_copy);
    EXPECT_NE(app_copy, app);

    app_copy->SetName("New Name");
    EXPECT_EQ(app_copy->name(), "New Name");
    EXPECT_EQ(app->name(), "Name");

    app_copy->AddSource(Source::kPolicy);
    app_copy->RemoveSource(Source::kSync);

    EXPECT_FALSE(app_copy->IsSynced());
    EXPECT_TRUE(app_copy->HasAnySources());

    EXPECT_TRUE(app->IsSynced());
    EXPECT_TRUE(app->HasAnySources());

    SyncBridgeCommitUpdate(std::move(update));
  }

  // Pointer value stays the same.
  EXPECT_EQ(app, registrar().GetAppById(app_id));

  EXPECT_EQ(app->name(), "New Name");
  EXPECT_FALSE(app->IsSynced());
  EXPECT_TRUE(app->HasAnySources());
}

TEST_F(WebAppRegistrarTest, CountUserInstalledApps) {
  controller().Init();

  const std::string base_url{"https://example.com/path"};

  for (int i = Source::kMinValue + 1; i <= Source::kMaxValue; ++i) {
    auto source = static_cast<Source::Type>(i);
    auto web_app =
        CreateWebAppWithSource(base_url + base::NumberToString(i), source);
    RegisterApp(std::move(web_app));
  }

  EXPECT_EQ(2, registrar().CountUserInstalledApps());
}

TEST_F(WebAppRegistrarTest, AppsInSyncInstallExcludedFromGetAppIds) {
  InitRegistrarWithApps("https://example.com/path/", 100);

  EXPECT_EQ(100u, registrar().GetAppIds().size());

  std::unique_ptr<WebApp> web_app_in_sync_install =
      CreateWebApp("https://example.org/");
  web_app_in_sync_install->SetIsInSyncInstall(true);

  const AppId web_app_in_sync_install_id = web_app_in_sync_install->app_id();
  RegisterApp(std::move(web_app_in_sync_install));

  // Tests that GetAppIds() excludes web app in sync install:
  std::vector<AppId> ids = registrar().GetAppIds();
  EXPECT_EQ(100u, ids.size());
  for (const AppId& app_id : ids)
    EXPECT_NE(app_id, web_app_in_sync_install_id);

  // Tests that AllApps() returns a web app which is either in GetAppIds() set
  // or it is the web app in sync install:
  bool web_app_in_sync_install_found = false;
  for (const WebApp& web_app : registrar().AllApps()) {
    if (web_app.app_id() == web_app_in_sync_install_id)
      web_app_in_sync_install_found = true;
    else
      EXPECT_TRUE(base::Contains(ids, web_app.app_id()));
  }
  EXPECT_TRUE(web_app_in_sync_install_found);
}

TEST_F(WebAppRegistrarTest, NotLocallyInstalledAppGetsDisplayModeBrowser) {
  controller().Init();

  auto web_app = CreateWebApp("https://example.com/path");
  const AppId app_id = web_app->app_id();
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetIsLocallyInstalled(false);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));

  sync_bridge().SetAppIsLocallyInstalled(app_id, true);

  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest_DisplayOverride,
       NotLocallyInstalledAppGetsDisplayModeOverride) {
  controller().Init();

  auto web_app = CreateWebApp("https://example.com/path");
  const AppId app_id = web_app->app_id();
  std::vector<DisplayMode> display_mode_overrides;
  display_mode_overrides.push_back(DisplayMode::kFullscreen);
  display_mode_overrides.push_back(DisplayMode::kMinimalUi);

  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetDisplayModeOverride(display_mode_overrides);
  web_app->SetIsLocallyInstalled(false);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));

  sync_bridge().SetAppIsLocallyInstalled(app_id, true);

  EXPECT_EQ(DisplayMode::kMinimalUi,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest_DisplayOverride,
       CheckDisplayOverrideFromGetEffectiveDisplayModeFromManifest) {
  controller().Init();

  auto web_app = CreateWebApp("https://example.com/path");
  const AppId app_id = web_app->app_id();
  std::vector<DisplayMode> display_mode_overrides;
  display_mode_overrides.push_back(DisplayMode::kFullscreen);
  display_mode_overrides.push_back(DisplayMode::kMinimalUi);

  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetDisplayModeOverride(display_mode_overrides);
  web_app->SetIsLocallyInstalled(false);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kFullscreen,
            registrar().GetEffectiveDisplayModeFromManifest(app_id));

  sync_bridge().SetAppIsLocallyInstalled(app_id, true);
  EXPECT_EQ(DisplayMode::kFullscreen,
            registrar().GetEffectiveDisplayModeFromManifest(app_id));
}

TEST_F(WebAppRegistrarTest, RunOnOsLoginModes) {
  controller().Init();

  auto web_app = CreateWebApp("https://example.com/path");
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));

  EXPECT_EQ(RunOnOsLoginMode::kUndefined,
            registrar().GetAppRunOnOsLoginMode(app_id));

  sync_bridge().SetAppRunOnOsLoginMode(app_id, RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar().GetAppRunOnOsLoginMode(app_id));

  sync_bridge().SetAppRunOnOsLoginMode(app_id, RunOnOsLoginMode::kMinimized);
  EXPECT_EQ(RunOnOsLoginMode::kMinimized,
            registrar().GetAppRunOnOsLoginMode(app_id));
}

}  // namespace web_app
