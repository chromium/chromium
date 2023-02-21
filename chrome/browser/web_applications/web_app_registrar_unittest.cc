// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/url_constants.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/web_applications/test/with_crosapi_param.h"

using web_app::test::CrosapiParam;
using web_app::test::WithCrosapiParam;
#endif

namespace web_app {

namespace {

Registry CreateRegistryForTesting(const std::string& base_url, int num_apps) {
  Registry registry;

  for (int i = 0; i < num_apps; ++i) {
    const auto url = base_url + base::NumberToString(i);
    const AppId app_id =
        GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(url));

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(WebAppManagement::kSync);
    web_app->SetStartUrl(GURL(url));
    web_app->SetName("Name" + base::NumberToString(i));
    web_app->SetDisplayMode(DisplayMode::kBrowser);
    web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);

    registry.emplace(app_id, std::move(web_app));
  }

  return registry;
}

int CountApps(const WebAppRegistrar::AppSet& app_set) {
  int count = 0;
  for (const auto& web_app : app_set) {
    EXPECT_FALSE(web_app.is_uninstalling());
    ++count;
  }
  return count;
}

}  // namespace

class WebAppRegistrarTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    command_manager_ =
        std::make_unique<WebAppCommandManager>(profile(), provider);
    command_scheduler_ =
        std::make_unique<WebAppCommandScheduler>(*profile(), provider);
    registrar_mutable_ = std::make_unique<WebAppRegistrarMutable>(profile());
    sync_bridge_ = std::make_unique<WebAppSyncBridge>(
        registrar_mutable_.get(), mock_processor_.CreateForwardingProcessor());
    database_factory_ = std::make_unique<FakeWebAppDatabaseFactory>();
    install_manager_ = std::make_unique<WebAppInstallManager>(profile());

    sync_bridge_->SetSubsystems(database_factory_.get(), command_manager_.get(),
                                command_scheduler_.get(),
                                install_manager_.get());

    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

 protected:
  void DestroyManagers() {
    if (command_manager_) {
      command_manager_->Shutdown();
      command_manager_.reset();
    }
    if (command_scheduler_) {
      command_scheduler_->Shutdown();
      command_scheduler_.reset();
    }
    if (registrar_mutable_) {
      registrar_mutable_.reset();
    }
    if (sync_bridge_) {
      sync_bridge_.reset();
    }
    if (database_factory_) {
      database_factory_.reset();
    }
  }

  FakeWebAppDatabaseFactory& database_factory() { return *database_factory_; }
  WebAppRegistrar& registrar() { return *registrar_mutable_; }
  WebAppRegistrarMutable& mutable_registrar() { return *registrar_mutable_; }
  WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }

  base::flat_set<AppId> RegisterAppsForTesting(Registry registry) {
    base::flat_set<AppId> ids;

    ScopedRegistryUpdate update(&sync_bridge());
    for (auto& kv : registry) {
      ids.insert(kv.first);
      update->CreateApp(std::move(kv.second));
    }

    return ids;
  }

  void RegisterApp(std::unique_ptr<WebApp> web_app) {
    ScopedRegistryUpdate update(&sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  void UnregisterApp(const AppId& app_id) {
    ScopedRegistryUpdate update(&sync_bridge());
    update->DeleteApp(app_id);
  }

  void UnregisterAll() {
    ScopedRegistryUpdate update(&sync_bridge());
    for (const AppId& app_id : registrar().GetAppIds())
      update->DeleteApp(app_id);
  }

  AppId InitRegistrarWithApp(std::unique_ptr<WebApp> app) {
    DCHECK(registrar().is_empty());

    AppId app_id = app->app_id();

    Registry registry;
    registry.emplace(app_id, std::move(app));

    InitRegistrarWithRegistry(registry);
    return app_id;
  }

  base::flat_set<AppId> InitRegistrarWithApps(const std::string& base_url,
                                              int num_apps) {
    DCHECK(registrar().is_empty());

    Registry registry = CreateRegistryForTesting(base_url, num_apps);
    return InitRegistrarWithRegistry(registry);
  }

  base::flat_set<AppId> InitRegistrarWithRegistry(const Registry& registry) {
    base::flat_set<AppId> app_ids;
    for (auto& kv : registry)
      app_ids.insert(kv.second->app_id());

    database_factory().WriteRegistry(registry);
    InitSyncBridge();

    return app_ids;
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

  void InitSyncBridge() {
    base::RunLoop loop;
    sync_bridge_->Init(loop.QuitClosure());
    loop.Run();
  }

 private:
  std::unique_ptr<WebAppRegistrarMutable> registrar_mutable_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
  std::unique_ptr<FakeWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppCommandManager> command_manager_;
  std::unique_ptr<WebAppCommandScheduler> command_scheduler_;
  std::unique_ptr<WebAppInstallManager> install_manager_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
};

class WebAppRegistrarTest_TabStrip : public WebAppRegistrarTest {
 public:
  WebAppRegistrarTest_TabStrip() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kDesktopPWAsTabStrip};
};

TEST_F(WebAppRegistrarTest, CreateRegisterUnregister) {
  InitSyncBridge();

  EXPECT_EQ(nullptr, registrar().GetAppById(AppId()));
  EXPECT_FALSE(registrar().GetAppById(AppId()));

  const GURL start_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const absl::optional<SkColor> theme_color = 0xAABBCCDD;

  const GURL start_url2 = GURL("https://example.com/path2");
  const AppId app_id2 =
      GenerateAppId(/*manifest_id=*/absl::nullopt, start_url2);

  auto web_app = std::make_unique<WebApp>(app_id);
  auto web_app2 = std::make_unique<WebApp>(app_id2);

  web_app->AddSource(WebAppManagement::kSync);
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetName(name);
  web_app->SetDescription(description);
  web_app->SetStartUrl(start_url);
  web_app->SetScope(scope);
  web_app->SetThemeColor(theme_color);

  web_app2->AddSource(WebAppManagement::kDefault);
  web_app2->SetDisplayMode(DisplayMode::kBrowser);
  web_app2->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  web_app2->SetStartUrl(start_url2);
  web_app2->SetName(name);

  EXPECT_EQ(nullptr, registrar().GetAppById(app_id));
  EXPECT_EQ(nullptr, registrar().GetAppById(app_id2));
  EXPECT_TRUE(registrar().is_empty());

  RegisterApp(std::move(web_app));
  EXPECT_TRUE(registrar().IsInstalled(app_id));
  const WebApp* app = registrar().GetAppById(app_id);

  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(name, app->untranslated_name());
  EXPECT_EQ(description, app->untranslated_description());
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
  EXPECT_EQ(CountApps(registrar().GetApps()), 2);

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
  EXPECT_EQ(CountApps(registrar().GetApps()), 0);
}

TEST_F(WebAppRegistrarTest, DestroyRegistrarOwningRegisteredApps) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  RegisterApp(std::move(web_app));

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/path2"));
  RegisterApp(std::move(web_app2));

  DestroyManagers();
}

TEST_F(WebAppRegistrarTest, InitRegistrarAndDoForEachApp) {
  base::flat_set<AppId> ids =
      InitRegistrarWithApps("https://example.com/path", 100);

  for (const WebApp& web_app : registrar().GetAppsIncludingStubs()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, GetAppsIncludingStubsMutable) {
  base::flat_set<AppId> ids =
      InitRegistrarWithApps("https://example.com/path", 10);

  for (WebApp& web_app : mutable_registrar().GetAppsIncludingStubsMutable()) {
    web_app.SetDisplayMode(DisplayMode::kStandalone);
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, DoForEachAndUnregisterAllApps) {
  InitSyncBridge();

  Registry registry = CreateRegistryForTesting("https://example.com/path", 100);
  auto ids = RegisterAppsForTesting(std::move(registry));
  EXPECT_EQ(100UL, ids.size());

  for (const WebApp& web_app : registrar().GetAppsIncludingStubs()) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());

  EXPECT_FALSE(registrar().is_empty());
  UnregisterAll();
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, FilterApps) {
  InitSyncBridge();

  Registry registry = CreateRegistryForTesting("https://example.com/path", 100);
  auto ids = RegisterAppsForTesting(std::move(registry));

  for ([[maybe_unused]] const WebApp& web_app :
       mutable_registrar().FilterAppsMutableForTesting(
           [](const WebApp& web_app) { return false; })) {
    NOTREACHED();
  }

  for (const WebApp& web_app : mutable_registrar().FilterAppsMutableForTesting(
           [](const WebApp& web_app) { return true; })) {
    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, GetApps) {
  base::flat_set<AppId> ids =
      InitRegistrarWithApps("https://example.com/path", 10);

  int not_in_sync_install_count = 0;
  for (const WebApp& web_app : registrar().GetApps()) {
    ++not_in_sync_install_count;
    EXPECT_TRUE(base::Contains(ids, web_app.app_id()));
  }
  EXPECT_EQ(10, not_in_sync_install_count);

  auto web_app_in_sync1 = test::CreateWebApp(GURL("https://example.org/sync1"));
  web_app_in_sync1->SetIsFromSyncAndPendingInstallation(true);
  const AppId web_app_id_in_sync1 = web_app_in_sync1->app_id();
  RegisterApp(std::move(web_app_in_sync1));

  auto web_app_in_sync2 = test::CreateWebApp(GURL("https://example.org/sync2"));
  web_app_in_sync2->SetIsFromSyncAndPendingInstallation(true);
  const AppId web_app_id_in_sync2 = web_app_in_sync2->app_id();
  RegisterApp(std::move(web_app_in_sync2));

  int all_apps_count = 0;
  for ([[maybe_unused]] const WebApp& web_app :
       registrar().GetAppsIncludingStubs()) {
    ++all_apps_count;
  }
  EXPECT_EQ(12, all_apps_count);

  for (const WebApp& web_app : registrar().GetApps()) {
    EXPECT_NE(web_app_id_in_sync1, web_app.app_id());
    EXPECT_NE(web_app_id_in_sync2, web_app.app_id());

    const size_t num_removed = ids.erase(web_app.app_id());
    EXPECT_EQ(1U, num_removed);
  }
  EXPECT_TRUE(ids.empty());

  UnregisterApp(web_app_id_in_sync1);
  UnregisterApp(web_app_id_in_sync2);

  not_in_sync_install_count = 0;
  for ([[maybe_unused]] const WebApp& web_app : registrar().GetApps()) {
    ++not_in_sync_install_count;
  }
  EXPECT_EQ(10, not_in_sync_install_count);
}

TEST_F(WebAppRegistrarTest, WebAppSyncBridge) {
  base::flat_set<AppId> ids =
      InitRegistrarWithApps("https://example.com/path", 100);

  // Add 1 app after Init.
  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  const AppId app_id = web_app->app_id();

  RegisterApp(std::move(web_app));

  EXPECT_EQ(101UL, database_factory().ReadAllAppIds().size());
  EXPECT_EQ(101UL, mutable_registrar().registry().size());

  // Remove 1 app after Init.
  UnregisterApp(app_id);
  EXPECT_EQ(100UL, mutable_registrar().registry().size());
  EXPECT_EQ(100UL, database_factory().ReadAllAppIds().size());

  // Remove 100 apps after Init.
  UnregisterAll();
  EXPECT_TRUE(database_factory().ReadAllAppIds().empty());
  EXPECT_TRUE(registrar().is_empty());
}

TEST_F(WebAppRegistrarTest, GetAppDataFields) {
  InitSyncBridge();

  const GURL start_url = GURL("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  const std::string name = "Name";
  const std::string description = "Description";
  const absl::optional<SkColor> theme_color = 0xAABBCCDD;
  const auto display_mode = DisplayMode::kMinimalUi;
  const auto user_display_mode = mojom::UserDisplayMode::kStandalone;
  std::vector<DisplayMode> display_mode_override;

  EXPECT_EQ(std::string(), registrar().GetAppShortName(app_id));
  EXPECT_EQ(GURL(), registrar().GetAppStartUrl(app_id));

  auto web_app = std::make_unique<WebApp>(app_id);
  WebApp* web_app_ptr = web_app.get();

  display_mode_override.push_back(DisplayMode::kMinimalUi);
  display_mode_override.push_back(DisplayMode::kStandalone);

  web_app->AddSource(WebAppManagement::kSync);
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
  EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
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
    EXPECT_FALSE(registrar().IsActivelyInstalled(app_id));

    EXPECT_FALSE(registrar().IsLocallyInstalled("unknown"));
    web_app_ptr->SetIsLocallyInstalled(/*is_locally_installed*/ true);
    EXPECT_TRUE(registrar().IsLocallyInstalled(app_id));
    EXPECT_TRUE(registrar().IsActivelyInstalled(app_id));
  }

  {
    EXPECT_FALSE(registrar().GetAppUserDisplayMode("unknown").has_value());

    web_app_ptr->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
              registrar().GetAppUserDisplayMode(app_id));

    sync_bridge().SetAppUserDisplayMode(app_id,
                                        mojom::UserDisplayMode::kStandalone,
                                        /*is_user_action=*/false);
    EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
              web_app_ptr->user_display_mode());
    EXPECT_EQ(DisplayMode::kMinimalUi, web_app_ptr->display_mode());

    ASSERT_EQ(2u, web_app_ptr->display_mode_override().size());
    EXPECT_EQ(DisplayMode::kMinimalUi, web_app_ptr->display_mode_override()[0]);
    EXPECT_EQ(DisplayMode::kStandalone,
              web_app_ptr->display_mode_override()[1]);
  }
}

TEST_F(WebAppRegistrarTest, CanFindAppsInScope) {
  InitSyncBridge();

  const GURL origin_scope("https://example.com/");

  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");

  const AppId app1_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app1_scope);
  const AppId app2_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app2_scope);
  const AppId app3_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app3_scope);

  std::vector<AppId> in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_EQ(0u, in_scope.size());
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(origin_scope));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(app3_scope));

  auto app1 = test::CreateWebApp(app1_scope);
  app1->SetScope(app1_scope);
  RegisterApp(std::move(app1));

  in_scope = registrar().FindAppsInScope(origin_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(origin_scope));
  EXPECT_FALSE(registrar().DoesScopeContainAnyApp(app3_scope));

  in_scope = registrar().FindAppsInScope(app1_scope);
  EXPECT_THAT(in_scope, testing::UnorderedElementsAre(app1_id));
  EXPECT_TRUE(registrar().DoesScopeContainAnyApp(app1_scope));

  auto app2 = test::CreateWebApp(app2_scope);
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

  auto app3 = test::CreateWebApp(app3_scope);
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
  InitSyncBridge();

  const GURL origin_scope("https://example.com/");

  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");
  const GURL app3_scope("https://not-example.com/app");
  const GURL app4_scope("https://app-four.com/");

  const AppId app1_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app1_scope);
  const AppId app2_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app2_scope);
  const AppId app3_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app3_scope);
  const AppId app4_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app3_scope);

  auto app1 = test::CreateWebApp(app1_scope);
  app1->SetScope(app1_scope);
  RegisterApp(std::move(app1));

  absl::optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_scope);
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, app1_id);

  absl::optional<AppId> app3_match =
      registrar().FindAppWithUrlInScope(app3_scope);
  EXPECT_FALSE(app3_match);

  absl::optional<AppId> app4_match =
      registrar().FindAppWithUrlInScope(app4_scope);
  EXPECT_FALSE(app4_match);

  auto app2 = test::CreateWebApp(app2_scope);
  app2->SetScope(app2_scope);
  RegisterApp(std::move(app2));

  auto app3 = test::CreateWebApp(app3_scope);
  app3->SetScope(app3_scope);
  RegisterApp(std::move(app3));

  auto app4 = test::CreateWebApp(app4_scope);
  app4->SetScope(app4_scope);
  app4->SetIsUninstalling(true);
  RegisterApp(std::move(app4));

  absl::optional<AppId> origin_match =
      registrar().FindAppWithUrlInScope(origin_scope);
  EXPECT_FALSE(origin_match);

  absl::optional<AppId> app1_match =
      registrar().FindAppWithUrlInScope(app1_scope);
  DCHECK(app1_match);
  EXPECT_EQ(*app1_match, app1_id);

  app2_match = registrar().FindAppWithUrlInScope(app2_scope);
  DCHECK(app2_match);
  EXPECT_EQ(*app2_match, app2_id);

  app3_match = registrar().FindAppWithUrlInScope(app3_scope);
  DCHECK(app3_match);
  EXPECT_EQ(*app3_match, app3_id);

  // Apps in the process of uninstalling are ignored.
  app4_match = registrar().FindAppWithUrlInScope(app4_scope);
  EXPECT_FALSE(app4_match);
}

TEST_F(WebAppRegistrarTest, CanFindShortcutWithUrlInScope) {
  InitSyncBridge();

  const GURL app1_page("https://example.com/app/page");
  const GURL app2_page("https://example.com/app-two/page");
  const GURL app3_page("https://not-example.com/app/page");

  const GURL app1_launch("https://example.com/app/launch");
  const GURL app2_launch("https://example.com/app-two/launch");
  const GURL app3_launch("https://not-example.com/app/launch");

  const AppId app1_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app1_launch);
  const AppId app2_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app2_launch);
  const AppId app3_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app3_launch);

  // Implicit scope "https://example.com/app/"
  auto app1 = test::CreateWebApp(app1_launch);
  RegisterApp(std::move(app1));

  absl::optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_page);
  EXPECT_FALSE(app2_match);

  absl::optional<AppId> app3_match =
      registrar().FindAppWithUrlInScope(app3_page);
  EXPECT_FALSE(app3_match);

  auto app2 = test::CreateWebApp(app2_launch);
  RegisterApp(std::move(app2));

  auto app3 = test::CreateWebApp(app3_launch);
  RegisterApp(std::move(app3));

  absl::optional<AppId> app1_match =
      registrar().FindAppWithUrlInScope(app1_page);
  DCHECK(app1_match);
  EXPECT_EQ(app1_match, absl::optional<AppId>(app1_id));

  app2_match = registrar().FindAppWithUrlInScope(app2_page);
  DCHECK(app2_match);
  EXPECT_EQ(app2_match, absl::optional<AppId>(app2_id));

  app3_match = registrar().FindAppWithUrlInScope(app3_page);
  DCHECK(app3_match);
  EXPECT_EQ(app3_match, absl::optional<AppId>(app3_id));
}

TEST_F(WebAppRegistrarTest, FindPwaOverShortcut) {
  InitSyncBridge();

  const GURL app1_launch("https://example.com/app/specific/launch1");

  const GURL app2_scope("https://example.com/app");
  const GURL app2_page("https://example.com/app/specific/page2");
  const AppId app2_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, app2_scope);

  const GURL app3_launch("https://example.com/app/specific/launch3");

  auto app1 = test::CreateWebApp(app1_launch);
  RegisterApp(std::move(app1));

  auto app2 = test::CreateWebApp(app2_scope);
  app2->SetScope(app2_scope);
  RegisterApp(std::move(app2));

  auto app3 = test::CreateWebApp(app3_launch);
  RegisterApp(std::move(app3));

  absl::optional<AppId> app2_match =
      registrar().FindAppWithUrlInScope(app2_page);
  DCHECK(app2_match);
  EXPECT_EQ(app2_match, absl::optional<AppId>(app2_id));
}

TEST_F(WebAppRegistrarTest, BeginAndCommitUpdate) {
  base::flat_set<AppId> ids =
      InitRegistrarWithApps("https://example.com/path", 10);

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
    EXPECT_EQ("New Name", kv.second->untranslated_name());
    ids.erase(kv.second->app_id());
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, CommitEmptyUpdate) {
  base::flat_set<AppId> ids =
      InitRegistrarWithApps("https://example.com/path", 10);
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
  base::flat_set<AppId> ids =
      InitRegistrarWithApps("https://example.com/path", 10);
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
    EXPECT_EQ(kv.second->untranslated_description(), "New Description");
    ids.erase(kv.second->app_id());
  }

  EXPECT_TRUE(ids.empty());
}

TEST_F(WebAppRegistrarTest, CopyOnWrite) {
  InitSyncBridge();

  const GURL start_url("https://example.com");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  const WebApp* app = nullptr;
  {
    auto new_app = test::CreateWebApp(start_url);
    app = new_app.get();
    RegisterApp(std::move(new_app));
  }

  {
    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

    WebApp* app_copy = update->UpdateApp(app_id);
    EXPECT_TRUE(app_copy);
    EXPECT_NE(app_copy, app);

    app_copy->SetName("New Name");
    EXPECT_EQ(app_copy->untranslated_name(), "New Name");
    EXPECT_EQ(app->untranslated_name(), "Name");

    app_copy->AddSource(WebAppManagement::kPolicy);
    app_copy->RemoveSource(WebAppManagement::kSync);

    EXPECT_FALSE(app_copy->IsSynced());
    EXPECT_TRUE(app_copy->HasAnySources());

    EXPECT_TRUE(app->IsSynced());
    EXPECT_TRUE(app->HasAnySources());

    SyncBridgeCommitUpdate(std::move(update));
  }

  // Pointer value stays the same.
  EXPECT_EQ(app, registrar().GetAppById(app_id));

  EXPECT_EQ(app->untranslated_name(), "New Name");
  EXPECT_FALSE(app->IsSynced());
  EXPECT_TRUE(app->HasAnySources());
}

TEST_F(WebAppRegistrarTest, CountUserInstalledApps) {
  InitSyncBridge();

  const std::string base_url{"https://example.com/path"};

  for (int i = WebAppManagement::kMinValue + 1;
       i <= WebAppManagement::kMaxValue; ++i) {
    auto source = static_cast<WebAppManagement::Type>(i);
    auto web_app =
        test::CreateWebApp(GURL(base_url + base::NumberToString(i)), source);
    RegisterApp(std::move(web_app));
  }

  EXPECT_EQ(3, registrar().CountUserInstalledApps());
}

TEST_F(WebAppRegistrarTest, GetAllIsolatedWebAppStoragePartitionConfigs) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  InitSyncBridge();

  constexpr char kIwaHostname[] =
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
  GURL start_url(base::StrCat({chrome::kIsolatedAppScheme,
                               url::kStandardSchemeSeparator, kIwaHostname}));
  auto isolated_web_app = test::CreateWebApp(start_url);
  const AppId app_id = isolated_web_app->app_id();

  isolated_web_app->SetScope(isolated_web_app->start_url());
  isolated_web_app->SetIsolationData(
      WebApp::IsolationData(InstalledBundle{.path = base::FilePath()}));
  RegisterApp(std::move(isolated_web_app));

  std::vector<content::StoragePartitionConfig> storage_partition_configs =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(app_id);

  auto expected_config = content::StoragePartitionConfig::Create(
      profile(), /*partition_domain=*/base::StrCat({"iwa-", kIwaHostname}),
      /*partition_name=*/"", /*in_memory=*/false);
  ASSERT_EQ(1UL, storage_partition_configs.size());
  EXPECT_EQ(expected_config, storage_partition_configs[0]);
}

TEST_F(
    WebAppRegistrarTest,
    GetAllIsolatedWebAppStoragePartitionConfigsEmptyWhenNotLocallyInstalled) {
  base::test::ScopedFeatureList scoped_feature_list(features::kIsolatedWebApps);
  InitSyncBridge();

  GURL start_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  auto isolated_web_app = test::CreateWebApp(start_url);
  const AppId app_id = isolated_web_app->app_id();

  isolated_web_app->SetScope(isolated_web_app->start_url());
  isolated_web_app->SetIsolationData(
      WebApp::IsolationData(InstalledBundle{.path = base::FilePath()}));
  isolated_web_app->SetIsLocallyInstalled(false);
  RegisterApp(std::move(isolated_web_app));

  std::vector<content::StoragePartitionConfig> storage_partition_configs =
      registrar().GetIsolatedWebAppStoragePartitionConfigs(app_id);

  EXPECT_TRUE(storage_partition_configs.empty());
}

TEST_F(WebAppRegistrarTest,
       AppsFromSyncAndPendingInstallationExcludedFromGetAppIds) {
  InitRegistrarWithApps("https://example.com/path/", 100);

  EXPECT_EQ(100u, registrar().GetAppIds().size());

  std::unique_ptr<WebApp> web_app_in_sync_install =
      test::CreateWebApp(GURL("https://example.org/"));
  web_app_in_sync_install->SetIsFromSyncAndPendingInstallation(true);

  const AppId web_app_in_sync_install_id = web_app_in_sync_install->app_id();
  RegisterApp(std::move(web_app_in_sync_install));

  // Tests that GetAppIds() excludes web app in sync install:
  std::vector<AppId> ids = registrar().GetAppIds();
  EXPECT_EQ(100u, ids.size());
  for (const AppId& app_id : ids)
    EXPECT_NE(app_id, web_app_in_sync_install_id);

  // Tests that GetAppsIncludingStubs() returns a web app which is either in
  // GetAppIds() set or it is the web app in sync install:
  bool web_app_in_sync_install_found = false;
  for (const WebApp& web_app : registrar().GetAppsIncludingStubs()) {
    if (web_app.app_id() == web_app_in_sync_install_id)
      web_app_in_sync_install_found = true;
    else
      EXPECT_TRUE(base::Contains(ids, web_app.app_id()));
  }
  EXPECT_TRUE(web_app_in_sync_install_found);
}

TEST_F(WebAppRegistrarTest, NotLocallyInstalledAppGetsDisplayModeBrowser) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetIsLocallyInstalled(false);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));

  sync_bridge().SetAppIsLocallyInstalledForTesting(app_id, true);

  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest,
       NotLocallyInstalledAppGetsDisplayModeBrowserEvenForIsolatedWebApps) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetIsLocallyInstalled(false);

  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest,
       IsolatedWebAppsGetDisplayModeStandaloneRegardlessOfUserSettings) {
  InitSyncBridge();

  std::unique_ptr<WebApp> web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();

  // Valid manifest must have standalone display mode
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
  web_app->SetIsLocallyInstalled(true);
  web_app->SetIsolationData(WebApp::IsolationData(DevModeProxy{
      .proxy_url = url::Origin::Create(GURL("http://127.0.0.1:8080"))}));

  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest, NotLocallyInstalledAppGetsDisplayModeOverride) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  std::vector<DisplayMode> display_mode_overrides;
  display_mode_overrides.push_back(DisplayMode::kFullscreen);
  display_mode_overrides.push_back(DisplayMode::kMinimalUi);

  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetDisplayModeOverride(display_mode_overrides);
  web_app->SetIsLocallyInstalled(false);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kBrowser,
            registrar().GetAppEffectiveDisplayMode(app_id));

  sync_bridge().SetAppIsLocallyInstalledForTesting(app_id, true);

  EXPECT_EQ(DisplayMode::kMinimalUi,
            registrar().GetAppEffectiveDisplayMode(app_id));
}

TEST_F(WebAppRegistrarTest,
       CheckDisplayOverrideFromGetEffectiveDisplayModeFromManifest) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  std::vector<DisplayMode> display_mode_overrides;
  display_mode_overrides.push_back(DisplayMode::kFullscreen);
  display_mode_overrides.push_back(DisplayMode::kMinimalUi);

  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetDisplayModeOverride(display_mode_overrides);
  web_app->SetIsLocallyInstalled(false);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(DisplayMode::kFullscreen,
            registrar().GetEffectiveDisplayModeFromManifest(app_id));

  sync_bridge().SetAppIsLocallyInstalledForTesting(app_id, true);
  EXPECT_EQ(DisplayMode::kFullscreen,
            registrar().GetEffectiveDisplayModeFromManifest(app_id));
}

TEST_F(WebAppRegistrarTest, WindowControlsOverlay) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));

  EXPECT_EQ(false, registrar().GetWindowControlsOverlayEnabled(app_id));

  sync_bridge().SetAppWindowControlsOverlayEnabled(app_id, true);
  EXPECT_EQ(true, registrar().GetWindowControlsOverlayEnabled(app_id));

  sync_bridge().SetAppWindowControlsOverlayEnabled(app_id, false);
  EXPECT_EQ(false, registrar().GetWindowControlsOverlayEnabled(app_id));
}

TEST_F(WebAppRegistrarTest, IsRegisteredLaunchProtocol) {
  InitSyncBridge();

  apps::ProtocolHandlerInfo protocol_handler_info1;
  protocol_handler_info1.protocol = "web+test";
  protocol_handler_info1.url = GURL("http://example.com/test=%s");

  apps::ProtocolHandlerInfo protocol_handler_info2;
  protocol_handler_info2.protocol = "web+test2";
  protocol_handler_info2.url = GURL("http://example.com/test2=%s");

  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  const AppId app_id = web_app->app_id();
  web_app->SetProtocolHandlers(
      {protocol_handler_info1, protocol_handler_info2});
  RegisterApp(std::move(web_app));

  EXPECT_TRUE(registrar().IsRegisteredLaunchProtocol(app_id, "web+test"));
  EXPECT_TRUE(registrar().IsRegisteredLaunchProtocol(app_id, "web+test2"));
  EXPECT_FALSE(registrar().IsRegisteredLaunchProtocol(app_id, "web+test3"));
  EXPECT_FALSE(registrar().IsRegisteredLaunchProtocol(app_id, "mailto"));
}

TEST_F(WebAppRegistrarTest, TestIsDefaultManagementInstalled) {
  InitSyncBridge();

  auto web_app1 =
      test::CreateWebApp(GURL("https://start.com"), WebAppManagement::kDefault);
  auto web_app2 = test::CreateWebApp(GURL("https://starter.com"),
                                     WebAppManagement::kPolicy);
  const AppId app_id1 = web_app1->app_id();
  const AppId app_id2 = web_app2->app_id();
  RegisterApp(std::move(web_app1));
  RegisterApp(std::move(web_app2));

  // Currently default installed.
  EXPECT_TRUE(registrar().IsInstalledByDefaultManagement(app_id1));
  // Currently installed by source other than installed.
  EXPECT_FALSE(registrar().IsInstalledByDefaultManagement(app_id2));

  // Uninstalling the previously default installed app.
  UnregisterApp(app_id1);
  EXPECT_FALSE(registrar().IsInstalledByDefaultManagement(app_id1));
}

TEST_F(WebAppRegistrarTest, DefaultNotActivelyInstalled) {
  std::unique_ptr<WebApp> default_app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kDefault);
  default_app->SetDisplayMode(DisplayMode::kStandalone);
  default_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);

  const AppId app_id = default_app->app_id();
  const GURL external_app_url("https://example.com/path/default");

  Registry registry;
  registry.emplace(app_id, std::move(default_app));
  InitRegistrarWithRegistry(registry);

  EXPECT_FALSE(registrar().IsActivelyInstalled(app_id));
}

TEST_F(WebAppRegistrarTest_TabStrip, TabbedAppNewTabUrl) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  AppId app_id = web_app->app_id();
  GURL new_tab_url = GURL("https://example.com/path/newtab");

  blink::Manifest::NewTabButtonParams new_tab_button_params;
  new_tab_button_params.url = new_tab_url;
  TabStrip tab_strip;
  tab_strip.new_tab_button = new_tab_button_params;

  web_app->SetDisplayMode(DisplayMode::kTabbed);
  web_app->SetTabStrip(tab_strip);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(registrar().GetAppNewTabUrl(app_id), new_tab_url);
}

TEST_F(WebAppRegistrarTest_TabStrip, TabbedAppAutoNewTabUrl) {
  InitSyncBridge();

  auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
  AppId app_id = web_app->app_id();

  TabStrip tab_strip;
  tab_strip.new_tab_button = TabStrip::Visibility::kAuto;

  web_app->SetDisplayMode(DisplayMode::kTabbed);
  web_app->SetTabStrip(tab_strip);
  RegisterApp(std::move(web_app));

  EXPECT_EQ(registrar().GetAppNewTabUrl(app_id),
            registrar().GetAppStartUrl(app_id));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class WebAppRegistrarAshTest : public WebAppTest, public WithCrosapiParam {
 public:
  WebAppRegistrarAshTest() = default;
  ~WebAppRegistrarAshTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppRegistrarAshTest, SourceSupported) {
  const GURL example_url("https://example.com/my-app/start");
  const GURL swa_url("chrome://swa/start");
  const GURL uninstalling_url("https://example.com/uninstalling/start");

  AppId example_id;
  AppId swa_id;
  AppId uninstalling_id;
  WebAppRegistrarMutable registrar(profile());
  {
    Registry registry;

    auto example_app = test::CreateWebApp(example_url);
    example_id = example_app->app_id();
    registry.emplace(example_id, std::move(example_app));

    auto swa_app = test::CreateWebApp(swa_url, WebAppManagement::Type::kSystem);
    swa_id = swa_app->app_id();
    registry.emplace(swa_id, std::move(swa_app));

    auto uninstalling_app =
        test::CreateWebApp(uninstalling_url, WebAppManagement::Type::kSystem);
    uninstalling_app->SetIsUninstalling(true);
    uninstalling_id = uninstalling_app->app_id();
    registry.emplace(uninstalling_id, std::move(uninstalling_app));

    registrar.InitRegistry(std::move(registry));
  }

  if (GetParam() == CrosapiParam::kEnabled) {
    // Non-system web apps are managed by Lacros, excluded in Ash
    // WebAppRegistrar.
    EXPECT_EQ(registrar.CountUserInstalledApps(), 0);
    EXPECT_EQ(CountApps(registrar.GetApps()), 1);

    EXPECT_FALSE(registrar.FindAppWithUrlInScope(example_url).has_value());
    EXPECT_TRUE(registrar.GetAppScope(example_id).is_empty());
    EXPECT_FALSE(registrar.GetAppUserDisplayMode(example_id).has_value());
  } else {
    EXPECT_EQ(registrar.CountUserInstalledApps(), 1);
    EXPECT_EQ(CountApps(registrar.GetApps()), 2);

    EXPECT_EQ(registrar.FindAppWithUrlInScope(example_url), example_id);
    EXPECT_EQ(registrar.GetAppScope(example_id),
              GURL("https://example.com/my-app/"));
    EXPECT_TRUE(registrar.GetAppUserDisplayMode(example_id).has_value());
  }

  EXPECT_EQ(registrar.FindAppWithUrlInScope(swa_url), swa_id);
  EXPECT_EQ(registrar.GetAppScope(swa_id), GURL("chrome://swa/"));
  EXPECT_TRUE(registrar.GetAppUserDisplayMode(swa_id).has_value());

  EXPECT_FALSE(registrar.FindAppWithUrlInScope(uninstalling_url).has_value());
  EXPECT_EQ(registrar.GetAppScope(uninstalling_id),
            GURL("https://example.com/uninstalling/"));
  EXPECT_TRUE(registrar.GetAppUserDisplayMode(uninstalling_id).has_value());
  EXPECT_FALSE(base::Contains(registrar.GetAppIds(), uninstalling_id));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppRegistrarAshTest,
                         ::testing::Values(CrosapiParam::kEnabled,
                                           CrosapiParam::kDisabled),
                         WithCrosapiParam::ParamToString);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)

using WebAppRegistrarLacrosTest = WebAppTest;

TEST_F(WebAppRegistrarLacrosTest, SwaSourceNotSupported) {
  const GURL example_url("https://example.com/my-app/start");
  const GURL swa_url("chrome://swa/start");
  const GURL uninstalling_url("https://example.com/uninstalling/start");

  AppId example_id;
  AppId swa_id;
  AppId uninstalling_id;
  WebAppRegistrarMutable registrar(profile());
  {
    Registry registry;

    auto example_app = test::CreateWebApp(example_url);
    example_id = example_app->app_id();
    registry.emplace(example_id, std::move(example_app));

    auto swa_app = test::CreateWebApp(swa_url, WebAppManagement::Type::kSystem);
    swa_id = swa_app->app_id();
    registry.emplace(swa_id, std::move(swa_app));

    auto uninstalling_app = test::CreateWebApp(uninstalling_url);
    uninstalling_app->SetIsUninstalling(true);
    uninstalling_id = uninstalling_app->app_id();
    registry.emplace(uninstalling_id, std::move(uninstalling_app));

    registrar.InitRegistry(std::move(registry));
  }

  EXPECT_EQ(registrar.FindAppWithUrlInScope(example_url), example_id);
  EXPECT_EQ(registrar.GetAppScope(example_id),
            GURL("https://example.com/my-app/"));
  EXPECT_TRUE(registrar.GetAppUserDisplayMode(example_id).has_value());
  EXPECT_EQ(registrar.CountUserInstalledApps(), 1);

  // System web apps are managed by Ash, excluded in Lacros
  // WebAppRegistrar.
  EXPECT_EQ(CountApps(registrar.GetApps()), 1);

  EXPECT_FALSE(registrar.FindAppWithUrlInScope(swa_url).has_value());
  EXPECT_TRUE(registrar.GetAppScope(swa_id).is_empty());
  EXPECT_FALSE(registrar.GetAppUserDisplayMode(swa_id).has_value());

  EXPECT_FALSE(registrar.FindAppWithUrlInScope(uninstalling_url).has_value());
  EXPECT_EQ(registrar.GetAppScope(uninstalling_id),
            GURL("https://example.com/uninstalling/"));
  EXPECT_TRUE(registrar.GetAppUserDisplayMode(uninstalling_id).has_value());
  EXPECT_FALSE(base::Contains(registrar.GetAppIds(), uninstalling_id));
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace web_app
