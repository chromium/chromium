// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/app_service/browser_shortcuts.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kUrl[] = "https://example.com/";
}

namespace web_app {

class BrowserShortcutsTest : public testing::Test {
 public:
  // testing::Test implementation.
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCrosWebAppShortcutUiUpdate);
    profile_ = std::make_unique<TestingProfile>();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::string CreateShortcut(const std::string& shortcut_name) {
    const GURL kAppUrl(kUrl);

    // Create a web app entry without scope, which would be recognised
    // as ShortcutApp in the web app system.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(shortcut_name);
    web_app_info->start_url = kAppUrl;

    std::string app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    CHECK(
        WebAppProvider::GetForTest(profile())->registrar_unsafe().IsShortcutApp(
            app_id));
    return app_id;
  }

  std::string CreateWebApp(const std::string& app_name) {
    const GURL kAppUrl(kUrl);

    // Create a web app entry with scope, which would be recognised
    // as normal web app in the web app system.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->start_url = kAppUrl;
    web_app_info->scope = kAppUrl;

    std::string app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    CHECK(!WebAppProvider::GetForTest(profile())
               ->registrar_unsafe()
               .IsShortcutApp(app_id));
    return app_id;
  }

  void InitializeBrowserShortcutPublisher() {
    base::RunLoop run_loop;
    web_app::BrowserShortcuts::SetInitializedCallbackForTesting(
        run_loop.QuitClosure());
    apps::AppServiceTest app_service_test;
    app_service_test.SetUp(profile());
    run_loop.Run();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BrowserShortcutsTest, PublishExistingBrowserShortcut) {
  const std::string kShortcutName = "Shortcut";

  auto local_shortcut_id = CreateShortcut(kShortcutName);
  apps::ShortcutId expected_shortcut_id =
      apps::GenerateShortcutId(app_constants::kChromeAppId, local_shortcut_id);
  InitializeBrowserShortcutPublisher();

  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  ASSERT_EQ(cache->GetAllShortcuts().size(), 1u);
  ASSERT_TRUE(cache->HasShortcut(expected_shortcut_id));

  apps::ShortcutView stored_shortcut = cache->GetShortcut(expected_shortcut_id);
  ASSERT_TRUE(stored_shortcut);
  EXPECT_EQ(stored_shortcut->shortcut_id, expected_shortcut_id);
  EXPECT_EQ(stored_shortcut->name, "Shortcut");
  EXPECT_EQ(stored_shortcut->shortcut_source, apps::ShortcutSource::kUser);
  EXPECT_EQ(stored_shortcut->host_app_id, app_constants::kChromeAppId);
  EXPECT_EQ(stored_shortcut->local_id, local_shortcut_id);
}

TEST_F(BrowserShortcutsTest, WebAppNotPublishedAsShortcut) {
  const std::string kAppName = "App";

  auto app_id = CreateWebApp(kAppName);

  InitializeBrowserShortcutPublisher();

  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  EXPECT_EQ(cache->GetAllShortcuts().size(), 0u);
}

}  // namespace web_app
