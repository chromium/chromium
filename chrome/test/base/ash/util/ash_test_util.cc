// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/util/ash_test_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash::test {

namespace {

// Returns the path of the downloads mount point associated with the `profile`.
base::FilePath GetDownloadsPath(Profile* profile) {
  base::FilePath result;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(profile), &result));
  return result;
}

aura::Window* GetRootWindow(const views::View* view) {
  return view->GetWidget()->GetNativeWindow()->GetRootWindow();
}

// Creates the app launch params for `app_type` for testing.
apps::AppLaunchParams GetAppLaunchParams(Profile* profile,
                                         ash::SystemWebAppType app_type) {
  std::optional<webapps::AppId> app_id =
      ash::GetAppIdForSystemWebApp(profile, app_type);
  if (!app_id) {
    NOTREACHED() << "To launch system web apps, you should first call "
                    "ash::SystemWebAppManager::InstallSystemAppsForTesting in "
                    "your test.";
  }
  return apps::AppLaunchParams(
      *app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
}

webapps::AppId CreateSystemWebAppImpl(Profile* profile,
                                      apps::AppLaunchParams params) {
  const webapps::AppId app_id = params.app_id;
  base::RunLoop launch_wait;
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithParams(
      std::move(params),
      base::BindLambdaForTesting(
          [&](apps::LaunchResult&& result) { launch_wait.Quit(); }));
  launch_wait.Run();
  return app_id;
}

}  // namespace

void Click(const views::View* view, int flags) {
  ui::test::EventGenerator event_generator(GetRootWindow(view));
  event_generator.set_flags(flags);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.PressModifierKeys(flags);
  event_generator.ClickLeftButton();
  event_generator.ReleaseModifierKeys(flags);
}

base::FilePath CreateFile(Profile* profile, std::string_view extension) {
  const base::FilePath file_path =
      GetDownloadsPath(profile).Append(base::StrCat(
          {base::UnguessableToken::Create().ToString(), ".", extension}));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::CreateDirectory(file_path.DirName())) {
      ADD_FAILURE() << "Failed to create parent directory.";
      return base::FilePath();
    }
    if (!base::WriteFile(file_path, /*data=*/std::string())) {
      ADD_FAILURE() << "Filed to write file contents.";
      return base::FilePath();
    }
  }

  return file_path;
}

void MoveMouseTo(const views::View* view, size_t count) {
  ui::test::EventGenerator(GetRootWindow(view))
      .MoveMouseTo(view->GetBoundsInScreen().CenterPoint(), count);
}

void InstallSystemAppsForTesting(Profile* profile) {
  ash::SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();
}

webapps::AppId CreateSystemWebApp(Profile* profile,
                                  ash::SystemWebAppType app_type,
                                  std::optional<int32_t> window_id) {
  apps::AppLaunchParams params = GetAppLaunchParams(profile, app_type);
  if (window_id) {
    params.restore_id = *window_id;
  }
  return CreateSystemWebAppImpl(profile, std::move(params));
}

Browser* CreateBrowser(Profile* profile,
                       const std::vector<GURL>& urls,
                       std::optional<size_t> active_url_index) {
  Browser::CreateParams params(Browser::TYPE_NORMAL, profile,
                               /*user_gesture=*/false);
  Browser* browser = Browser::Create(params);
  // Create a new tab and make sure the urls have loaded.
  for (size_t i = 0; i < urls.size(); i++) {
    content::TestNavigationObserver navigation_observer(urls[i]);
    navigation_observer.StartWatchingNewWebContents();
    chrome::AddTabAt(
        browser, urls[i], /*index=*/-1,
        /*foreground=*/!active_url_index || active_url_index.value() == i);
    navigation_observer.Wait();
  }
  return browser;
}

Browser* CreateAndShowBrowser(Profile* profile,
                              const std::vector<GURL>& urls,
                              std::optional<size_t> active_url_index) {
  Browser* browser = CreateBrowser(profile, urls, active_url_index);
  browser->window()->Show();
  return browser;
}

Browser* InstallAndLaunchPWA(Profile* profile,
                             const GURL& start_url,
                             bool launch_in_browser,
                             const std::u16string& app_title) {
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url.GetWithoutFilename();
  if (!launch_in_browser) {
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
  }
  web_app_info->title = app_title;
  const webapps::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(web_app_info));

  return launch_in_browser
             ? web_app::LaunchBrowserForWebAppInTab(profile, app_id)
             : web_app::LaunchWebAppBrowserAndWait(profile, app_id);
}

BrowsersWaiter::BrowsersWaiter(int expected_count)
    : expected_count_(expected_count) {
  BrowserList::AddObserver(this);
}

BrowsersWaiter::~BrowsersWaiter() {
  BrowserList::RemoveObserver(this);
}

void BrowsersWaiter::Wait() {
  run_loop_.Run();
}

void BrowsersWaiter::OnBrowserAdded(Browser* browser) {
  ++current_count_;
  if (current_count_ == expected_count_) {
    run_loop_.Quit();
  }
}

}  // namespace ash::test
