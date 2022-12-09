// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/url_handler_manager_impl.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/url_handler_prefs.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_switches.h"
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
  UrlHandlerManagerImplTest() {
    features_.InitAndEnableFeature(blink::features::kWebAppEnableUrlHandlers);
  }
  ~UrlHandlerManagerImplTest() override = default;

 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    // This is not a WebAppProvider subsystem, so this can be
    // set after the WebAppProvider has been initialized.
    auto url_handler_manager =
        std::make_unique<UrlHandlerManagerImpl>(profile());
    url_handler_manager_ = url_handler_manager.get();
    url_handler_manager->SetSubsystems(&provider_->registrar_unsafe());

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

    fake_os_integration_manager().SetUrlHandlerManager(
        std::move(url_handler_manager));
  }

  void TearDown() override { WebAppTest::TearDown(); }

  FakeOsIntegrationManager& fake_os_integration_manager() {
    return static_cast<FakeOsIntegrationManager&>(
        provider_->os_integration_manager());
  }

  WebAppSyncBridge& sync_bridge() { return provider_->sync_bridge(); }

  UrlHandlerManagerImpl& url_handler_manager() { return *url_handler_manager_; }

  std::unique_ptr<WebApp> CreateWebAppWithUrlHandlers(
      const GURL& app_url,
      const apps::UrlHandlers& url_handlers) {
    const std::string app_id =
        GenerateAppId(/*manifest_id=*/absl::nullopt, app_url);
    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->AddSource(WebAppManagement::kDefault);
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(UserDisplayMode::kStandalone);
    web_app->SetName("Name");
    web_app->SetStartUrl(app_url);
    web_app->SetUrlHandlers(url_handlers);
    return web_app;
  }

  AppId RegisterAppAndUrlHandlers() {
    auto web_app = CreateWebAppWithUrlHandlers(
        app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
    const AppId& app_id = web_app->app_id();

    {
      ScopedRegistryUpdate update(&sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    base::RunLoop run_loop;
    url_handler_manager().RegisterUrlHandlers(
        app_id, base::BindLambdaForTesting([&](Result result) {
          EXPECT_EQ(Result::kOk, result);
          run_loop.Quit();
        }));
    run_loop.Run();
    return app_id;
  }

  AppId UpdateAppAndUrlHandlers() {
    auto web_app = CreateWebAppWithUrlHandlers(
        app_url_1_, {apps::UrlHandlerInfo(origin_2_)});
    const AppId& app_id = web_app->app_id();
    {
      ScopedRegistryUpdate update(&sync_bridge());
      update->DeleteApp(app_id);
    }
    {
      ScopedRegistryUpdate update(&sync_bridge());
      update->CreateApp(std::move(web_app));
    }
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
  raw_ptr<FakeWebAppProvider> provider_;
  raw_ptr<UrlHandlerManagerImpl> url_handler_manager_;
  base::test::ScopedFeatureList features_;
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
  {
    ScopedRegistryUpdate update(&sync_bridge());
    update->DeleteApp(app_id);
  }

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

TEST_F(UrlHandlerManagerImplTest, GetUrlHandlerMatches_CommandlineNoMatch) {
  const AppId app_Id = RegisterAppAndUrlHandlers();
  base::CommandLine cmd = base::CommandLine(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg("https://origin-1.com/foo");
  auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(cmd);
  EXPECT_TRUE(matches.empty());
}

}  // namespace web_app
