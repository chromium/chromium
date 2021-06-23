// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"

#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/url_handler_prefs.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

constexpr char kAppUrl1[] = "https://web-app1.com/";
constexpr char kOriginUrl1[] = "https://origin-1.com/abc";
constexpr char kOriginUrl2[] = "https://origin-2.com/abc";

}  // namespace

class UrlHandlerManagerImplTest : public WebAppTest {
 public:
  UrlHandlerManagerImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
    features_.InitAndEnableFeature(blink::features::kWebAppEnableUrlHandlers);
  }
  ~UrlHandlerManagerImplTest() override = default;

 protected:
  void SetUp() override {
    WebAppTest::SetUp();
    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());

    auto url_handler_manager =
        std::make_unique<UrlHandlerManagerImpl>(profile());
    url_handler_manager_ = url_handler_manager.get();
    url_handler_manager->SetSubsystems(&test_registry_controller_->registrar());

    auto association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    std::map<apps::UrlHandlerInfo, apps::UrlHandlerInfo> data = {
        {apps::UrlHandlerInfo(origin_1_),
         apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {"/foo"})},
        {apps::UrlHandlerInfo(origin_2_),
         apps::UrlHandlerInfo(origin_2_, false, {"/abc"}, {"/bar"})},
    };
    association_manager->SetData(std::move(data));
    url_handler_manager->SetAssociationManagerForTesting(
        std::move(association_manager));

    test_os_integration_manager().SetUrlHandlerManager(
        std::move(url_handler_manager));
    test_registry_controller_->Init();
  }

  void TearDown() override { WebAppTest::TearDown(); }

  TestOsIntegrationManager& test_os_integration_manager() {
    return test_registry_controller_->os_integration_manager();
  }

  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  UrlHandlerManagerImpl& url_handler_manager() { return *url_handler_manager_; }

  std::unique_ptr<WebApp> CreateWebAppWithUrlHandlers(
      const GURL& app_url,
      const apps::UrlHandlers& url_handlers) {
    const std::string app_id = GenerateAppIdFromURL(app_url);
    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(Source::kDefault);
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    web_app->SetName("Name");
    web_app->SetStartUrl(app_url);
    web_app->SetUrlHandlers(url_handlers);
    return web_app;
  }

  AppId RegisterAppAndUrlHandlers() {
    auto web_app = CreateWebAppWithUrlHandlers(
        app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
    const AppId& app_id = web_app->app_id();

    controller().RegisterApp(std::move(web_app));

    base::RunLoop run_loop;
    url_handler_manager().RegisterUrlHandlers(
        app_id, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
    return app_id;
  }

  AppId UpdateAppAndUrlHandlers() {
    auto web_app = CreateWebAppWithUrlHandlers(
        app_url_1_, {apps::UrlHandlerInfo(origin_2_)});
    const AppId& app_id = web_app->app_id();

    controller().UnregisterApp(app_id);
    controller().RegisterApp(std::move(web_app));

    base::RunLoop run_loop;
    url_handler_manager().UpdateUrlHandlers(
        app_id, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
    return app_id;
  }

  const GURL app_url_1_ = GURL(kAppUrl1);
  const GURL origin_url_1_ = GURL(kOriginUrl1);
  const GURL origin_url_2_ = GURL(kOriginUrl2);
  const url::Origin origin_1_ = url::Origin::Create(origin_url_1_);
  const url::Origin origin_2_ = url::Origin::Create(origin_url_2_);

 private:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  UrlHandlerManagerImpl* url_handler_manager_;
  ScopedTestingLocalState local_state_;
};

TEST_F(UrlHandlerManagerImplTest, RegisterAndUnregisterApp) {
  const AppId app_id = RegisterAppAndUrlHandlers();
  base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg(kOriginUrl1);

  // Find URL in cmd and look for matching URL handlers.
  std::vector<UrlHandlerLaunchParams> matches =
      UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  ASSERT_EQ(matches.size(), 1u);
  const UrlHandlerLaunchParams& params = matches[0];
  EXPECT_EQ(params.profile_path, profile()->GetPath());
  EXPECT_EQ(params.app_id, app_id);
  EXPECT_EQ(params.url, kOriginUrl1);

  // Unregister URL handlers, remove app.
  url_handler_manager().UnregisterUrlHandlers(app_id);
  controller().UnregisterApp(app_id);

  // Confirm there is no matching URL handler now.
  matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  EXPECT_TRUE(matches.empty());
}

TEST_F(UrlHandlerManagerImplTest, RegisterAndUpdateApp) {
  base::CommandLine cmd_1 = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd_1.AppendArg(kOriginUrl1);
  AppId app_id;
  {
    app_id = RegisterAppAndUrlHandlers();

    // Find URL in commandline and look for matching URL handlers.
    std::vector<UrlHandlerLaunchParams> matches =
        UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd_1);

    ASSERT_EQ(matches.size(), 1u);
    const UrlHandlerLaunchParams& params = matches[0];
    EXPECT_EQ(params.profile_path, profile()->GetPath());
    EXPECT_EQ(params.app_id, app_id);
    EXPECT_EQ(params.url, kOriginUrl1);
  }
  {
    AppId updated_app_id = UpdateAppAndUrlHandlers();
    EXPECT_EQ(app_id, updated_app_id);

    // Expect that url handlers targeting origin 1 have been replaced.
    std::vector<UrlHandlerLaunchParams> matches =
        UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd_1);
    EXPECT_TRUE(matches.empty());
  }
  {
    base::CommandLine cmd_2 = base::CommandLine(base::CommandLine::NO_PROGRAM);
    cmd_2.AppendArg(kOriginUrl2);

    std::vector<UrlHandlerLaunchParams> matches =
        UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd_2);

    // Expect new url handlers that target origin 2.
    ASSERT_EQ(matches.size(), 1u);
    const UrlHandlerLaunchParams& params = matches[0];
    EXPECT_EQ(params.profile_path, profile()->GetPath());
    EXPECT_EQ(params.app_id, app_id);
    EXPECT_EQ(params.url, kOriginUrl2);

    // Check excluded path shouldn't match
    cmd_2 = base::CommandLine(base::CommandLine::NO_PROGRAM);
    cmd_2.AppendArg("https://origin-2.com/bar");
    matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd_2);
    EXPECT_TRUE(matches.empty());
  }
}

TEST_F(UrlHandlerManagerImplTest, GetUrlHandlerMatches_CommandlineHasNoUrl) {
  const AppId app_Id = RegisterAppAndUrlHandlers();
  base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  EXPECT_TRUE(matches.empty());
}

TEST_F(UrlHandlerManagerImplTest,
       GetUrlHandlerMatches_CommandlineHasAppSwitches) {
  const AppId app_id = RegisterAppAndUrlHandlers();
  {
    base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
    cmd.AppendSwitchASCII(switches::kAppId, app_id);
    cmd.AppendArg(kOriginUrl1);

    auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
    EXPECT_TRUE(matches.empty());
  }
  {
    base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
    cmd.AppendSwitchASCII(switches::kApp, kAppUrl1);
    cmd.AppendArg(kOriginUrl1);

    auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
    EXPECT_TRUE(matches.empty());
  }
}

TEST_F(UrlHandlerManagerImplTest, FeatureFlagDisabled_Register) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(blink::features::kWebAppEnableUrlHandlers);

  auto web_app = CreateWebAppWithUrlHandlers(app_url_1_,
                                             {apps::UrlHandlerInfo(origin_1_)});
  const AppId& app_id = web_app->app_id();
  controller().RegisterApp(std::move(web_app));
  base::RunLoop run_loop;
  // Expect early return if feature is disabled.
  url_handler_manager().RegisterUrlHandlers(
      app_id, base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UrlHandlerManagerImplTest, FeatureFlagDisabled_Unregister) {
  AppId app_id = RegisterAppAndUrlHandlers();

  // Disable feature and try to unregister url handlers.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(blink::features::kWebAppEnableUrlHandlers);
    EXPECT_TRUE(url_handler_manager().UnregisterUrlHandlers(app_id));
  }

  // Check that url handlers are no longer registered.
  base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg(kOriginUrl1);
  auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  ASSERT_EQ(matches.size(), 0u);
}

TEST_F(UrlHandlerManagerImplTest, FeatureFlagDisabled_Update) {
  AppId app_id = RegisterAppAndUrlHandlers();

  // Disable feature and try to update url handlers.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(blink::features::kWebAppEnableUrlHandlers);

    auto web_app = CreateWebAppWithUrlHandlers(
        app_url_1_, {apps::UrlHandlerInfo(origin_2_)});
    EXPECT_EQ(app_id, web_app->app_id());

    controller().UnregisterApp(app_id);
    controller().RegisterApp(std::move(web_app));

    base::RunLoop run_loop;
    url_handler_manager().UpdateUrlHandlers(
        app_id, base::BindLambdaForTesting([&](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
  }

  // Expect that url handlers have been removed.
  base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg(kOriginUrl1);
  auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  ASSERT_EQ(matches.size(), 0u);
}

TEST_F(UrlHandlerManagerImplTest, GetUrlHandlerMatches_CommandlineNoMatch) {
  const AppId app_Id = RegisterAppAndUrlHandlers();
  base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg("https://origin-1.com/foo");
  auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  EXPECT_TRUE(matches.empty());
}

}  // namespace web_app
