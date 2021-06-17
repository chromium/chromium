// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_client.h"

#include <cstdint>
#include <memory>
#include <string>

#include "ash/public/cpp/desks_helper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/features.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/restore_data.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace {

constexpr int32_t kSettingsWindowId = 100;

constexpr char kExampleUrl1[] = "https://examples.com";
constexpr char kExampleUrl2[] = "https://examples.com";

aura::Window* FindAppWindow(int32_t window_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    if (window->GetProperty(::full_restore::kRestoreWindowIdKey) == window_id)
      return window;
  }
  return nullptr;
}

std::vector<GURL> GetURLsForBrowserWindow(Browser* browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  std::vector<GURL> urls;
  for (int i = 0; i < tab_strip_model->count(); ++i)
    urls.push_back(tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  return urls;
}

}  // namespace

class DesksClientTest : public InProcessBrowserTest {
 public:
  DesksClientTest() {
    // This feature depends on full restore feature, so need to enable it.
    scoped_feature_list_.InitAndEnableFeature(
        full_restore::features::kFullRestore);
  }
  ~DesksClientTest() override = default;

  void SetUpOnMainThread() override {
    ::full_restore::SetActiveProfilePath(browser()->profile()->GetPath());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a browser's urls can be captured correctly in the desk template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureBrowserUrlsTest) {
  // Create a new browser and add a few tabs to it.
  Browser::CreateParams params(Browser::TYPE_NORMAL, browser()->profile(),
                               /*user_gesture=*/false);
  Browser* browser = Browser::Create(params);
  chrome::AddTabAt(browser, GURL(kExampleUrl1), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser, GURL(kExampleUrl2), /*index=*/-1,
                   /*foreground=*/true);
  browser->window()->Show();
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(::full_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      DesksClient::Get()->CaptureActiveDeskAsTemplate();

  ASSERT_TRUE(desk_template);
  full_restore::RestoreData* restore_data = desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(extension_misc::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Check the urls are captured correctly in the |desk_template|.
  EXPECT_EQ(data->urls.value(), urls);
}

// Tests that browsers and chrome apps can be captured correctly in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureActiveDeskAsTemplateTest) {
  // Test that Singleton was properly initialized.
  ASSERT_TRUE(DesksClient::Get());
  Profile* profile = browser()->profile();

  // Change |browser|'s bounds.
  const gfx::Rect browser_bounds = gfx::Rect(0, 0, 800, 200);
  aura::Window* window = browser()->window()->GetNativeWindow();
  window->SetBounds(browser_bounds);
  // Make window visible on all desks.
  window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey, true);
  const int32_t browser_window_id =
      window->GetProperty(::full_restore::kWindowIdKey);

  // Create a Chrome settings app.
  web_app::WebAppProvider::Get(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
  web_app::AppId settings_app_id = *web_app::GetAppIdForSystemWebApp(
      profile, web_app::SystemAppType::SETTINGS);
  apps::AppLaunchParams params(
      settings_app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceTest);
  params.restore_id = kSettingsWindowId;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithParams(std::move(params));
  web_app::FlushSystemWebAppLaunchesForTesting(profile);

  // Change the Settings app's bounds too.
  const gfx::Rect settings_app_bounds = gfx::Rect(100, 100, 800, 300);
  aura::Window* settings_window = FindAppWindow(kSettingsWindowId);
  const int32_t settings_window_id =
      settings_window->GetProperty(::full_restore::kWindowIdKey);
  ASSERT_TRUE(settings_window);
  settings_window->SetBounds(settings_app_bounds);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      DesksClient::Get()->CaptureActiveDeskAsTemplate();
  ASSERT_TRUE(desk_template);

  // Test the default template's name is the current desk's name.
  auto* desks_helper = ash::DesksHelper::Get();
  EXPECT_EQ(desk_template->desk_name(),
            desks_helper->GetDeskName(desks_helper->GetActiveDeskIndex()));

  full_restore::RestoreData* restore_data = desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 2u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(extension_misc::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Verify window info are correctly captured.
  EXPECT_EQ(browser_bounds, data->current_bounds.value());
  EXPECT_EQ(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey),
            data->visible_on_all_workspaces.value());
  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  // We don't capture the window's desk_id as a template will always create in
  // a new desk.
  EXPECT_FALSE(data->desk_id.has_value());

  // Find Setting app's app restore data.
  auto iter2 = app_id_to_launch_list.find(settings_app_id);
  ASSERT_TRUE(iter2 != app_id_to_launch_list.end());
  auto app_restore_data_iter2 = iter2->second.find(settings_window_id);
  ASSERT_TRUE(app_restore_data_iter2 != iter2->second.end());
  const auto& data2 = app_restore_data_iter2->second;
  EXPECT_EQ(
      static_cast<int>(apps::mojom::LaunchContainer::kLaunchContainerWindow),
      data2->container.value());
  EXPECT_EQ(static_cast<int>(WindowOpenDisposition::NEW_WINDOW),
            data2->disposition.value());
  // Verify window info are correctly captured.
  EXPECT_EQ(settings_app_bounds, data2->current_bounds.value());
  EXPECT_FALSE(data2->visible_on_all_workspaces.has_value());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_FALSE(data2->desk_id.has_value());
}
