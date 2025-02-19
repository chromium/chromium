// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

namespace web_app {

namespace {
#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kCurrentDirectory[] =
    FILE_PATH_LITERAL("\\path");
#else
const base::FilePath::CharType kCurrentDirectory[] = FILE_PATH_LITERAL("/path");
#endif  // BUILDFLAG(IS_WIN)

class LaunchWebAppWithFirstRunServiceBrowserTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LaunchWebAppWithFirstRunServiceBrowserTest() = default;
  ~LaunchWebAppWithFirstRunServiceBrowserTest() override = default;

 protected:
  WebAppProvider& GetProvider() {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  webapps::AppId InstallWebApp(const GURL& app_url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

    webapps::AppId app_id;
    base::RunLoop run_loop;
    GetProvider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const webapps::AppId& new_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
          app_id = new_app_id;
          run_loop.Quit();
        }),
        FallbackBehavior::kAllowFallbackDataAlways);

    run_loop.Run();
    return app_id;
  }
};

IN_PROC_BROWSER_TEST_P(
    LaunchWebAppWithFirstRunServiceBrowserTest,
    LaunchInWindowWithFirstRunServiceRequiredSetupSuccessful) {
  webapps::AppId app_id =
      InstallWebApp(https_server()->GetURL("/banners/manifest_test_page.html"));

  ASSERT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
            GetProvider().registrar_unsafe().GetInstallState(app_id));

  Browser* browser = LaunchWebAppBrowser(app_id);
  ASSERT_TRUE(browser);
}

IN_PROC_BROWSER_TEST_P(LaunchWebAppWithFirstRunServiceBrowserTest,
                       LaunchInTabWithFirstRunServiceRequiredSetupSuccessful) {
  webapps::AppId app_id =
      InstallWebApp(https_server()->GetURL("/banners/manifest_test_page.html"));

  ASSERT_EQ(proto::INSTALLED_WITH_OS_INTEGRATION,
            GetProvider().registrar_unsafe().GetInstallState(app_id));

  Browser* browser = LaunchBrowserForWebAppInTab(app_id);
  ASSERT_TRUE(browser);
}

INSTANTIATE_TEST_SUITE_P(All,
                         LaunchWebAppWithFirstRunServiceBrowserTest,
                         ::testing::Values(true, false));

class LaunchWebAppCommandTest : public WebAppBrowserTestBase {
 public:
  const std::string kAppName = "TestApp";
  const GURL kAppStartUrl = GURL("https://example.com");

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    app_id_ = test::InstallDummyWebApp(profile(), kAppName, kAppStartUrl);
  }

 protected:
  WebAppProvider& provider() {
    return *web_app::WebAppProvider::GetForTest(profile());
  }

  std::tuple<base::WeakPtr<Browser>,
             base::WeakPtr<content::WebContents>,
             apps::LaunchContainer>
  DoLaunch(apps::AppLaunchParams params) {
    base::test::TestFuture<base::WeakPtr<Browser>,
                           base::WeakPtr<content::WebContents>,
                           apps::LaunchContainer>
        future;
    provider().scheduler().LaunchAppWithCustomParams(std::move(params),
                                                     future.GetCallback());
    return future.Get();
  }

  base::CommandLine CreateCommandLine() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kAppId, app_id_);
    return command_line;
  }

  apps::AppLaunchParams CreateLaunchParams(
      webapps::AppId app_id,
      apps::LaunchContainer container,
      WindowOpenDisposition disposition,
      apps::LaunchSource source,
      const std::vector<base::FilePath>& launch_files,
      const std::optional<GURL>& url_handler_launch_url,
      const std::optional<GURL>& protocol_handler_launch_url) {
    apps::AppLaunchParams params(app_id, container, disposition, source);
    params.current_directory = base::FilePath(kCurrentDirectory);
    params.command_line = CreateCommandLine();
    params.launch_files = launch_files;
    params.url_handler_launch_url = url_handler_launch_url;
    params.protocol_handler_launch_url = protocol_handler_launch_url;

    return params;
  }

  webapps::AppId app_id_;
};

IN_PROC_BROWSER_TEST_F(LaunchWebAppCommandTest, TabbedLaunchCurrentBrowser) {
  apps::AppLaunchParams launch_params = CreateLaunchParams(
      app_id_, apps::LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::LaunchSource::kFromCommandLine, {}, std::nullopt, std::nullopt);

  base::WeakPtr<Browser> launch_browser;
  base::WeakPtr<content::WebContents> web_contents;
  apps::LaunchContainer launch_container;
  std::tie(launch_browser, web_contents, launch_container) =
      DoLaunch(std::move(launch_params));

  EXPECT_FALSE(AppBrowserController::IsWebApp(launch_browser.get()));
  EXPECT_EQ(launch_browser.get(), browser());
  EXPECT_EQ(launch_browser->tab_strip_model()->count(), 2);
  EXPECT_EQ(web_contents->GetVisibleURL(), kAppStartUrl);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAppCommandTest, StandaloneLaunch) {
  apps::AppLaunchParams launch_params = CreateLaunchParams(
      app_id_, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB, apps::LaunchSource::kFromCommandLine,
      {}, std::nullopt, std::nullopt);

  base::WeakPtr<Browser> launch_browser;
  base::WeakPtr<content::WebContents> web_contents;
  apps::LaunchContainer launch_container;
  std::tie(launch_browser, web_contents, launch_container) =
      DoLaunch(std::move(launch_params));

  EXPECT_TRUE(AppBrowserController::IsWebApp(launch_browser.get()));
  EXPECT_NE(launch_browser.get(), browser());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2ul);
  EXPECT_EQ(launch_browser->tab_strip_model()->count(), 1);
  EXPECT_EQ(web_contents->GetVisibleURL(), kAppStartUrl);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAppCommandTest, StandaloneLaunchAppConfig) {
  base::WeakPtr<Browser> launch_browser;
  base::WeakPtr<content::WebContents> web_contents;
  apps::LaunchContainer launch_container;
  base::test::TestFuture<base::WeakPtr<Browser>,
                         base::WeakPtr<content::WebContents>,
                         apps::LaunchContainer>
      launch_future;
  provider().scheduler().LaunchApp(app_id_, std::nullopt,
                                   launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Wait());
  std::tie(launch_browser, web_contents, launch_container) =
      launch_future.Get();

  EXPECT_TRUE(AppBrowserController::IsWebApp(launch_browser.get()));
  EXPECT_NE(launch_browser.get(), browser());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2ul);
  EXPECT_EQ(launch_browser->tab_strip_model()->count(), 1);
  EXPECT_EQ(web_contents->GetVisibleURL(), kAppStartUrl);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAppCommandTest, AppLaunchNoIntegration) {
  const GURL kStartUrl =
      https_server()->GetURL("/banners/manifest_test_page.html");
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kStartUrl);
  web_app_info->scope = kStartUrl.GetWithoutFilename();
  web_app_info->title = u"Name";
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

  // Install & bypass os integration.
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  WebAppInstallParams params;
  params.add_to_applications_menu = false;
  params.add_to_desktop = false;
  params.add_to_quick_launch_bar = false;
  params.install_state = proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
  provider().scheduler().InstallFromInfoWithParams(
      std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      install_future.GetCallback(), params);
  ASSERT_TRUE(install_future.Wait());
  webapps::AppId app_id = install_future.Get<webapps::AppId>();

  // Check that OS integration NOT has occurred.
  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppCurrentOsIntegrationState(app_id)
                   ->has_shortcut());
  EXPECT_EQ(provider().registrar_unsafe().GetInstallState(app_id),
            proto::INSTALLED_WITHOUT_OS_INTEGRATION);

  base::test::TestFuture<base::WeakPtr<Browser>,
                         base::WeakPtr<content::WebContents>,
                         apps::LaunchContainer>
      launch_future;
  provider().scheduler().LaunchApp(app_id, std::nullopt,
                                   launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Wait());
  EXPECT_EQ(provider().registrar_unsafe().GetInstallState(app_id),
            proto::INSTALLED_WITH_OS_INTEGRATION);

  // Check the state is correct.
  EXPECT_TRUE(AppBrowserController::IsWebApp(
      launch_future.Get<base::WeakPtr<Browser>>().get()));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2ul);
  EXPECT_EQ(
      launch_future.Get<base::WeakPtr<content::WebContents>>()->GetVisibleURL(),
      kStartUrl);

  // Check that OS integration has occurred & the app is fully installed now.
  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppCurrentOsIntegrationState(app_id)
                  ->has_shortcut());
  EXPECT_EQ(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
            provider().registrar_unsafe().GetInstallState(app_id));
}

}  // namespace

}  // namespace web_app
