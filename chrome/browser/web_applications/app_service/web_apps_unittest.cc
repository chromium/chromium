// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/app_service/web_apps.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class WebAppsTest : public testing::Test {
 public:
  // testing::Test implementation.
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
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

  void InitializeWebAppPublisher() {
    apps::AppServiceTest app_service_test;
    app_service_test.SetUp(profile());
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebAppsTest, ShortcutNotPublishedAsWebApp) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCrosWebAppShortcutUiUpdate);
  apps::AppServiceTest app_service_test;
  app_service_test.SetUp(profile());
  auto app_id = CreateWebApp(GURL("https://example.com/"), "App");
  auto shortcut_id =
      CreateShortcut(GURL("https://example-shortcut.com/"), "Shortcut");

  // Reinitialize web app publisher to verify web app initialization only
  // publish web apps.
  apps::AppUpdateWaiter waiter(profile(), app_id);
  InitializeWebAppPublisher();
  waiter.Wait();

  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache();
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
TEST_F(WebAppsTest, ShortcutPublishedAsWebApp) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kCrosWebAppShortcutUiUpdate);
#endif
  auto app_id = CreateWebApp(GURL("https://example.com/"), "App");
  auto shortcut_id =
      CreateShortcut(GURL("https://example-shortcut.com/"), "Shortcut");

  // Reinitialize web app publisher to verify web app initialization only
  // publish web apps.
  apps::AppUpdateWaiter waiter(profile(), app_id);
  apps::AppUpdateWaiter shortcut_waiter(profile(), shortcut_id);
  InitializeWebAppPublisher();
  waiter.Wait();
  shortcut_waiter.Wait();

  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache();
  size_t num_app_after_web_app_init = cache.GetAllApps().size();

  // Install new web app and verify only web app get published.
  auto new_app_id = CreateWebApp(GURL("https://new-example.com/"), "NewApp");
  auto new_shortcut_id =
      CreateShortcut(GURL("https://new-example-shortcut.com/"), "NewShortcut");
  EXPECT_EQ(num_app_after_web_app_init + 2, cache.GetAllApps().size());
  EXPECT_EQ(cache.GetAppType(new_shortcut_id), apps::AppType::kWeb);
  EXPECT_EQ(cache.GetAppType(new_app_id), apps::AppType::kWeb);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebAppsTest, UninstallWebApp_AppServiceShortcutEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCrosWebAppShortcutUiUpdate);

  apps::AppServiceTest app_service_test;
  app_service_test.SetUp(profile());

  // Verify that web app can be installed and uninstalled as normal.
  auto web_app_id = CreateWebApp(GURL("https://example.com/"), "App");
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache();
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
}  // namespace web_app
