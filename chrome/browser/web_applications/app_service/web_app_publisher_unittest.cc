// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/url_constants.h"
#include "net/base/url_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#endif

namespace web_app {

// Test the publishing of web apps in all platforms, will test both
// lacros_web_apps_controller and web_apps.
class WebAppPublisherTest : public testing::Test {
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

  webapps::AppId CreateShortcut(const GURL& shortcut_url,
                                const std::string& shortcut_name) {
    return test::InstallShortcut(profile(), shortcut_name, shortcut_url);
  }

  std::string CreateWebApp(const GURL& app_url, const std::string& app_name) {
    // Create a web app entry with scope, which would be recognised
    // as normal web app in the web app system.
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
    web_app_info->title = base::UTF8ToUTF16(app_name);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)

class WebAppPublisherTest_Mall : public WebAppPublisherTest {
 public:
  WebAppPublisherTest_Mall() : WebAppPublisherTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kCrosMall},
        /*disabled_features=*/{chromeos::features::kCrosMallSwa});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

// Verifies that when the `kCrosMall` feature is enabled, launches of the Mall
// app have a "context" URL parameter appended.
TEST_F(WebAppPublisherTest_Mall, LaunchMallAppWithContext) {
  CreateWebApp(GURL(chromeos::kAppMallBaseUrl), "Mall");

  auto* provider = WebAppProvider::GetForTest(profile());

  base::test::TestFuture<apps::AppLaunchParams,
                         web_app::LaunchWebAppWindowSetting>
      app_launch_future;
  static_cast<web_app::FakeWebAppUiManager*>(&provider->ui_manager())
      ->SetOnLaunchWebAppCallback(app_launch_future.GetRepeatingCallback());

  proxy()->Launch(kMallAppId, 0, apps::LaunchSource::kFromTest);
  auto [params, setting] = app_launch_future.Take();

  ASSERT_TRUE(params.intent->url.has_value());

  std::string context_value;
  ASSERT_TRUE(net::GetValueForKeyInQuery(*params.intent->url, "context",
                                         &context_value));
  ASSERT_FALSE(context_value.empty());
}

#endif

}  // namespace web_app
