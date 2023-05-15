// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/command_line.h"
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
      apps::LaunchContainer container,
      WindowOpenDisposition disposition,
      apps::LaunchSource source,
      const std::vector<base::FilePath>& launch_files,
      const absl::optional<GURL>& url_handler_launch_url,
      const absl::optional<GURL>& protocol_handler_launch_url) {
    apps::AppLaunchParams params(app_id_, container, disposition, source);
    params.current_directory = base::FilePath(kCurrentDirectory);
    params.command_line = CreateCommandLine();
    params.launch_files = launch_files;
    params.url_handler_launch_url = url_handler_launch_url;
    params.protocol_handler_launch_url = protocol_handler_launch_url;

    return params;
  }

  AppId app_id_;
};

IN_PROC_BROWSER_TEST_F(LaunchWebAppCommandTest, TabbedLaunchCurrentBrowser) {
  apps::AppLaunchParams launch_params = CreateLaunchParams(
      apps::LaunchContainer::kLaunchContainerTab,
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
      apps::LaunchContainer::kLaunchContainerWindow,
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

}  // namespace
}  // namespace web_app
