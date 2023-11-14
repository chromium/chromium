// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/web_apps_with_shortcuts_test.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#endif

namespace web_app {

// Test the publishing of web apps in all platforms, will test both
// lacros_web_apps_controller and web_apps.
class WebAppPublisherTest : public testing::Test,
                            public WebAppsWithShortcutsTest {
 public:
  // testing::Test implementation.
  void SetUp() override {
    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::string CreateShortcut(const GURL& shortcut_url,
                             const std::string& shortcut_name) {
    // Create a web app entry without scope, which would be recognised
    // as ShortcutApp in the web app system.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(shortcut_name);
    web_app_info->start_url = shortcut_url;

    std::string app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    CHECK(
        WebAppProvider::GetForTest(profile())->registrar_unsafe().IsShortcutApp(
            app_id));
    return app_id;
  }

  std::string CreateWebApp(const GURL& app_url, const std::string& app_name) {
    // Create a web app entry with scope, which would be recognised
    // as normal web app in the web app system.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;

    std::string app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    CHECK(!WebAppProvider::GetForTest(profile())
               ->registrar_unsafe()
               .IsShortcutApp(app_id));
    return app_id;
  }

  apps::AppServiceProxy* proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  void PublishApp(apps::AppType type, const std::string& app_id) {
    std::vector<apps::AppPtr> app_deltas;
    auto app = apps::AppPublisher::MakeApp(
        type, app_id, apps::Readiness::kReady, "Some App Name",
        apps::InstallReason::kUser, apps::InstallSource::kSystem);
    app->window_mode = apps::WindowMode::kWindow;
    app->allow_uninstall = true;
    app_deltas.push_back(std::move(app));
    proxy()->OnApps(std::move(app_deltas), type,
                    /* should_notify_initialized */ true);
  }

  void InitializeWebAppPublisher() {
    apps::AppServiceTest app_service_test;
    app_service_test.SetUp(profile());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // For Lacros, we need the loopback crosapi to publish
    // the web app to app service proxy without actually connect to
    // crosapi in the test. AppServiceTest::SetUp will resets the
    // crosapi connections in the app service proxy, so we have to
    // set up the loopback crosapi after the setup. And we need to initialize
    // the web app controller after set up the loopback crosapi to publish
    // already installed web apps in the web app system.
    // TODO(b/307477703): Add the loopback crosapi and init in app service test.
    loopback_crosapi_ =
        std::make_unique<LoopbackCrosapiAppServiceProxy>(profile());
    proxy()->LacrosWebAppsControllerForTesting()->Init();
#endif
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<LoopbackCrosapiAppServiceProxy> loopback_crosapi_ = nullptr;
#endif
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(WebAppPublisherTest, ShortcutNotPublishedAsWebApp) {
  EnableCrosWebAppShortcutUiUpdate(true);
  apps::AppServiceTest app_service_test;
  app_service_test.SetUp(profile());
  auto app_id = CreateWebApp(GURL("https://example.com/"), "App");
  auto shortcut_id =
      CreateShortcut(GURL("https://example-shortcut.com/"), "Shortcut");

  // Reinitialize web app publisher to verify web app initialization only
  // publish web apps.
  InitializeWebAppPublisher();
  apps::AppReadinessWaiter(profile(), app_id).Await();

  apps::AppRegistryCache& cache = proxy()->AppRegistryCache();
  EXPECT_FALSE(cache.IsAppInstalled(shortcut_id));

  size_t num_app_after_web_app_init = cache.GetAllApps().size();

  // Install new web app and verify only web app get published.
  auto new_app_id = CreateWebApp(GURL("https://new-example.com/"), "NewApp");
  auto new_shortcut_id =
      CreateShortcut(GURL("https://new-example-shortcut.com/"), "NewShortcut");
  EXPECT_EQ(num_app_after_web_app_init + 1, cache.GetAllApps().size());
  EXPECT_EQ(cache.GetAppType(new_app_id), apps::AppType::kWeb);
}
#endif

// For non ChromeOS platforms or when the kCrosWebAppShortcutUiUpdate is off,
// we still want to publish shortcuts as web app. This is checking old behaviour
// does not break.
TEST_F(WebAppPublisherTest, ShortcutPublishedAsWebApp) {
  EnableCrosWebAppShortcutUiUpdate(false);
  auto app_id = CreateWebApp(GURL("https://example.com/"), "App");
  auto shortcut_id =
      CreateShortcut(GURL("https://example-shortcut.com/"), "Shortcut");

  // Reinitialize web app publisher to verify web app initialization publish
  // both web apps and shortcuts.
  InitializeWebAppPublisher();
  apps::AppReadinessWaiter(profile(), app_id).Await();
  apps::AppReadinessWaiter(profile(), shortcut_id).Await();

  apps::AppRegistryCache& cache = proxy()->AppRegistryCache();
  size_t num_app_after_web_app_init = cache.GetAllApps().size();

  // Install new web app and verify only web app get published.
  auto new_app_id = CreateWebApp(GURL("https://new-example.com/"), "NewApp");
  auto new_shortcut_id =
      CreateShortcut(GURL("https://new-example-shortcut.com/"), "NewShortcut");
  EXPECT_EQ(num_app_after_web_app_init + 2, cache.GetAllApps().size());
  EXPECT_EQ(cache.GetAppType(new_shortcut_id), apps::AppType::kWeb);
  EXPECT_EQ(cache.GetAppType(new_app_id), apps::AppType::kWeb);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(WebAppPublisherTest, UninstallWebApp_AppServiceShortcutEnabled) {
  EnableCrosWebAppShortcutUiUpdate(true);

  InitializeWebAppPublisher();

  // Verify that web app can be installed and uninstalled as normal.
  auto web_app_id = CreateWebApp(GURL("https://example.com/"), "App");
  apps::AppRegistryCache& cache = proxy()->AppRegistryCache();
  bool found = cache.ForOneApp(web_app_id, [](const apps::AppUpdate& update) {
    EXPECT_TRUE(apps_util::IsInstalled(update.Readiness()));
  });
  ASSERT_TRUE(found);

  web_app::test::UninstallWebApp(profile(), web_app_id);
  cache.ForOneApp(web_app_id, [](const apps::AppUpdate& update) {
    EXPECT_FALSE(apps_util::IsInstalled(update.Readiness()));
  });

  // Verify that shortcuts are not published to app registry cache on
  // installation and uninstallation.
  auto web_shortcut_id =
      CreateShortcut(GURL("https://shortcut_example.com/"), "App");

  found =
      cache.ForOneApp(web_shortcut_id, [](const apps::AppUpdate& update) {});
  ASSERT_FALSE(found);

  web_app::test::UninstallWebApp(profile(), web_shortcut_id);
  found =
      cache.ForOneApp(web_shortcut_id, [](const apps::AppUpdate& update) {});
  ASSERT_FALSE(found);
}

#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)

class WebAppPublisherTest_Shortstand
    : public WebAppPublisherTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAppPublisherTest_Shortstand() {
    scoped_feature_list_.InitWithFeatureState(
        chromeos::features::kCrosShortstand, GetParam());
  }

  ~WebAppPublisherTest_Shortstand() override = default;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppPublisherTest_Shortstand, GetMenuModel) {
  bool shortstand_enabled = WebAppPublisherTest_Shortstand::GetParam();
  InitializeWebAppPublisher();
  auto web_app_id = CreateWebApp(GURL("https://example.com/"), "App");

  // Call GetMenuModel to verify menu items are created correctly
  base::test::TestFuture<apps::MenuItems> result;
  proxy()->GetMenuModel(web_app_id, apps::MenuType::kAppList,
                        display::kDefaultDisplayId, result.GetCallback());

  apps::MenuItems menu_items = result.Take();

  ASSERT_EQ(3U, menu_items.items.size());

  // First item, the 'New Window' item, should be a Submenu when Shortstand is
  // disabled, and a Command when Shortstand is enabled.
  EXPECT_EQ(menu_items.items[0]->command_id, ash::LAUNCH_NEW);
  if (shortstand_enabled) {
    EXPECT_EQ(menu_items.items[0]->type, apps::MenuItemType::kCommand);
  } else {
    EXPECT_EQ(menu_items.items[0]->type, apps::MenuItemType::kSubmenu);
  }

  for (size_t i = 1; i < menu_items.items.size(); ++i) {
    EXPECT_EQ(menu_items.items[i]->type, apps::MenuItemType::kCommand);
  }
}

TEST_P(WebAppPublisherTest_Shortstand, GetMenuModelCrosapi) {
  bool shortstand_enabled = WebAppPublisherTest_Shortstand::GetParam();
  apps::WebAppsCrosapi web_apps_crosapi(proxy());
  PublishApp(apps::AppType::kWeb, "web_app_id");
  web_apps_crosapi.RegisterPublisher(apps::AppType::kWeb);

  // Call GetMenuModel to verify menu items are created correctly
  base::test::TestFuture<apps::MenuItems> result;
  proxy()->GetMenuModel("web_app_id", apps::MenuType::kAppList,
                        display::kDefaultDisplayId, result.GetCallback());

  apps::MenuItems menu_items = result.Take();

  ASSERT_EQ(3U, menu_items.items.size());

  // First item, the 'New Window' item, should be a Submenu when Shortstand is
  // disabled, and a Command when Shortstand is enabled.
  EXPECT_EQ(menu_items.items[0]->command_id, ash::LAUNCH_NEW);
  if (shortstand_enabled) {
    EXPECT_EQ(menu_items.items[0]->type, apps::MenuItemType::kCommand);
  } else {
    EXPECT_EQ(menu_items.items[0]->type, apps::MenuItemType::kSubmenu);
  }

  for (size_t i = 1; i < menu_items.items.size(); ++i) {
    EXPECT_EQ(menu_items.items[i]->type, apps::MenuItemType::kCommand);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    WebAppPublisherTest_Shortstand,
    testing::Bool());

#endif

}  // namespace web_app
