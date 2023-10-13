// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/commands/launch_web_app_command.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {
namespace {

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kCurrentDirectory[] =
    FILE_PATH_LITERAL("\\path");
#else
const base::FilePath::CharType kCurrentDirectory[] = FILE_PATH_LITERAL("/path");
#endif  // BUILDFLAG(IS_WIN)

class LaunchWebAppCommandTest : public WebAppControllerBrowserTest {
 public:
  const std::string kAppName = "TestApp";
  const GURL kAppStartUrl = GURL("https://example.com");

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
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
      const absl::optional<GURL>& url_handler_launch_url,
      const absl::optional<GURL>& protocol_handler_launch_url) {
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
      apps::LaunchSource::kFromCommandLine, {}, absl::nullopt, absl::nullopt);

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
      {}, absl::nullopt, absl::nullopt);

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
class LaunchWebAppCommandTest_Shortstand : public LaunchWebAppCommandTest {
 public:
  std::tuple<base::WeakPtr<Browser>,
             base::WeakPtr<content::WebContents>,
             apps::LaunchContainer>
  DoNonCustomLaunch(const webapps::AppId& app_id, const GURL& url) {
    base::test::TestFuture<base::WeakPtr<Browser>,
                           base::WeakPtr<content::WebContents>,
                           apps::LaunchContainer>
        future;
    provider().scheduler().LaunchUrlInApp(app_id, url, future.GetCallback());
    return future.Get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kCrosShortstand};
};

IN_PROC_BROWSER_TEST_F(LaunchWebAppCommandTest_Shortstand,
                       ShortcutLaunchInTab) {
  const GURL kShortcutUrl("https://www.shortcut-example.com");
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->title = u"TestShortcut";
  web_app_info->start_url = kShortcutUrl;
  webapps::AppId web_shortcut_id =
      test::InstallWebApp(profile(), std::move(web_app_info));

  {
    apps::AppLaunchParams launch_params = CreateLaunchParams(
        web_shortcut_id, apps::LaunchContainer::kLaunchContainerTab,
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        apps::LaunchSource::kFromCommandLine, {}, absl::nullopt, absl::nullopt);

    auto [launch_browser, web_contents, launch_container] =
        DoLaunch(std::move(launch_params));

    EXPECT_FALSE(AppBrowserController::IsWebApp(launch_browser.get()));
    EXPECT_EQ(launch_browser.get(), browser());
    EXPECT_EQ(launch_browser->tab_strip_model()->count(), 2);
    EXPECT_EQ(web_contents->GetVisibleURL(), kShortcutUrl);
    EXPECT_EQ(launch_container, apps::LaunchContainer::kLaunchContainerTab);
  }

  // Verify that setting launch params to window still launch in tab.
  {
    apps::AppLaunchParams launch_params = CreateLaunchParams(
        web_shortcut_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::LaunchSource::kFromCommandLine, {}, absl::nullopt, absl::nullopt);

    auto [launch_browser, web_contents, launch_container] =
        DoLaunch(std::move(launch_params));

    EXPECT_FALSE(AppBrowserController::IsWebApp(launch_browser.get()));
    EXPECT_EQ(launch_browser.get(), browser());
    EXPECT_EQ(launch_browser->tab_strip_model()->count(), 3);
    EXPECT_EQ(web_contents->GetVisibleURL(), kShortcutUrl);
    EXPECT_EQ(launch_container, apps::LaunchContainer::kLaunchContainerTab);
  }

  // Verify that launch with non custom params will launch in tab.
  {
    auto [launch_browser, web_contents, launch_container] =
        DoNonCustomLaunch(web_shortcut_id, kShortcutUrl);

    EXPECT_FALSE(AppBrowserController::IsWebApp(launch_browser.get()));
    EXPECT_EQ(launch_browser.get(), browser());
    EXPECT_EQ(launch_browser->tab_strip_model()->count(), 4);
    EXPECT_EQ(web_contents->GetVisibleURL(), kShortcutUrl);
    EXPECT_EQ(launch_container, apps::LaunchContainer::kLaunchContainerTab);
  }
}

IN_PROC_BROWSER_TEST_F(LaunchWebAppCommandTest_Shortstand,
                       WebAppLaunchInStandaloneWindow) {
  {
    apps::AppLaunchParams launch_params = CreateLaunchParams(
        app_id_, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::CURRENT_TAB,
        apps::LaunchSource::kFromCommandLine, {}, absl::nullopt, absl::nullopt);

    auto [launch_browser, web_contents, launch_container] =
        DoLaunch(std::move(launch_params));

    EXPECT_TRUE(AppBrowserController::IsWebApp(launch_browser.get()));
    EXPECT_NE(launch_browser.get(), browser());
    EXPECT_EQ(BrowserList::GetInstance()->size(), 2ul);
    EXPECT_EQ(launch_browser->tab_strip_model()->count(), 1);
    EXPECT_EQ(web_contents->GetVisibleURL(), kAppStartUrl);
    EXPECT_EQ(launch_container, apps::LaunchContainer::kLaunchContainerWindow);
  }

  // Verify that setting launch container to tab will still launch in window.
  {
    apps::AppLaunchParams launch_params = CreateLaunchParams(
        app_id_, apps::LaunchContainer::kLaunchContainerTab,
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        apps::LaunchSource::kFromCommandLine, {}, absl::nullopt, absl::nullopt);

    auto [launch_browser, web_contents, launch_container] =
        DoLaunch(std::move(launch_params));

    EXPECT_TRUE(AppBrowserController::IsWebApp(launch_browser.get()));
    EXPECT_NE(launch_browser.get(), browser());
    EXPECT_EQ(BrowserList::GetInstance()->size(), 3ul);
    EXPECT_EQ(launch_browser->tab_strip_model()->count(), 1);
    EXPECT_EQ(web_contents->GetVisibleURL(), kAppStartUrl);
    EXPECT_EQ(launch_container, apps::LaunchContainer::kLaunchContainerWindow);
  }

  // Verify that launch with non custom params will launch in window.
  {
    auto [launch_browser, web_contents, launch_container] =
        DoNonCustomLaunch(app_id_, kAppStartUrl);

    EXPECT_TRUE(AppBrowserController::IsWebApp(launch_browser.get()));
    EXPECT_NE(launch_browser.get(), browser());
    EXPECT_EQ(BrowserList::GetInstance()->size(), 4ul);
    EXPECT_EQ(launch_browser->tab_strip_model()->count(), 1);
    EXPECT_EQ(web_contents->GetVisibleURL(), kAppStartUrl);
    EXPECT_EQ(launch_container, apps::LaunchContainer::kLaunchContainerWindow);
  }
}
#endif
}  // namespace
}  // namespace web_app
