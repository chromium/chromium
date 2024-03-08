// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/style/icon_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/faster_split_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_utils.h"

namespace {

void ClickButton(const views::Button* button) {
  CHECK(button);
  CHECK(button->GetVisible());
  aura::Window* root_window =
      button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseToInHost(button->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

const GURL& GetActiveUrl(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL();
}

}  // namespace

class FasterSplitScreenBrowserTest : public InProcessBrowserTest {
 public:
  FasterSplitScreenBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kFasterSplitScreenSetup},
        /*disabled_features=*/{});
  }
  FasterSplitScreenBrowserTest(const FasterSplitScreenBrowserTest&) = delete;
  FasterSplitScreenBrowserTest& operator=(const FasterSplitScreenBrowserTest&) =
      delete;
  ~FasterSplitScreenBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that if partial overview is active, and a window gets session
// restore'd, partial overview auto-snaps the window. See b/314816288.
IN_PROC_BROWSER_TEST_F(FasterSplitScreenBrowserTest,
                       AutoSnapWhileInSessionRestore) {
  // Create two browser windows and snap `window1` to start partial overview.
  aura::Window* window1 = browser()->window()->GetNativeWindow();
  ash::WindowState* window_state = ash::WindowState::Get(window1);
  CreateBrowser(browser()->profile());

  const ash::WindowSnapWMEvent primary_snap_event(
      ash::WM_EVENT_SNAP_PRIMARY, ash::WindowSnapActionSource::kTest);
  window_state->OnWMEvent(&primary_snap_event);
  ash::WaitForOverviewEntered();
  ASSERT_TRUE(ash::OverviewController::Get()->InOverviewSession());

  // Open a new browser window. Test it gets auto-snapped.
  Browser* browser3 = CreateBrowser(browser()->profile());
  aura::Window* window3 = browser3->window()->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window3)->IsSnapped());
  EXPECT_FALSE(ash::OverviewController::Get()->InOverviewSession());
}

class FasterSplitScreenWithNewSettingsBrowserTest
    : public FasterSplitScreenBrowserTest {
 public:
  FasterSplitScreenWithNewSettingsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kOsSettingsRevampWayfinding},
        /*disabled_features=*/{});
  }
  FasterSplitScreenWithNewSettingsBrowserTest(
      const FasterSplitScreenWithNewSettingsBrowserTest&) = delete;
  FasterSplitScreenWithNewSettingsBrowserTest& operator=(
      const FasterSplitScreenWithNewSettingsBrowserTest&) = delete;
  ~FasterSplitScreenWithNewSettingsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FasterSplitScreenWithNewSettingsBrowserTest,
                       SnapWindowWithNewSettings) {
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  // Create two browser windows and snap `window` to start partial overview.
  aura::Window* window = browser()->window()->GetNativeWindow();
  CreateBrowser(browser()->profile());
  ash::WindowState* window_state = ash::WindowState::Get(window);
  const ash::WindowSnapWMEvent primary_snap_event(
      ash::WM_EVENT_SNAP_PRIMARY, ash::WindowSnapActionSource::kTest);
  window_state->OnWMEvent(&primary_snap_event);
  ash::WaitForOverviewEntered();
  ASSERT_TRUE(ash::OverviewController::Get()->InOverviewSession());

  // Partial overview contains the settings button.
  auto* overview_grid =
      ash::OverviewController::Get()->overview_session()->GetGridWithRootWindow(
          window->GetRootWindow());
  ASSERT_TRUE(overview_grid);
  auto* faster_split_view = overview_grid->GetFasterSplitView();
  ASSERT_TRUE(faster_split_view);
  auto* settings_button = faster_split_view->settings_button();
  ASSERT_TRUE(settings_button);

  // Setup navigation observer to wait for the OS Settings page.
  constexpr char kOsSettingsUrl[] =
      "chrome://os-settings/systemPreferences?settingId=1900";
  GURL os_settings(kOsSettingsUrl);
  content::TestNavigationObserver navigation_observer(os_settings);
  navigation_observer.StartWatchingNewWebContents();

  // Click the overview settings button.
  ClickButton(settings_button);

  // Wait for OS Settings to open.
  navigation_observer.Wait();

  // Verify correct OS Settings page is opened.
  Browser* settings_browser = ash::FindSystemWebAppBrowser(
      browser()->profile(), ash::SystemWebAppType::SETTINGS);
  ASSERT_TRUE(settings_browser);
  ASSERT_EQ(os_settings, GetActiveUrl(settings_browser));
}

class FasterSplitScreenWithOldSettingsBrowserTest
    : public FasterSplitScreenBrowserTest {
 public:
  FasterSplitScreenWithOldSettingsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{ash::features::kOsSettingsRevampWayfinding});
  }
  FasterSplitScreenWithOldSettingsBrowserTest(
      const FasterSplitScreenWithOldSettingsBrowserTest&) = delete;
  FasterSplitScreenWithOldSettingsBrowserTest& operator=(
      const FasterSplitScreenWithOldSettingsBrowserTest&) = delete;
  ~FasterSplitScreenWithOldSettingsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FasterSplitScreenWithOldSettingsBrowserTest,
                       SnapWindowWithOldSettings) {
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  // Create two browser windows and snap `window` to start partial overview.
  aura::Window* window = browser()->window()->GetNativeWindow();
  CreateBrowser(browser()->profile());
  ash::WindowState* window_state = ash::WindowState::Get(window);
  const ash::WindowSnapWMEvent primary_snap_event(
      ash::WM_EVENT_SNAP_PRIMARY, ash::WindowSnapActionSource::kTest);
  window_state->OnWMEvent(&primary_snap_event);
  ash::WaitForOverviewEntered();
  ASSERT_TRUE(ash::OverviewController::Get()->InOverviewSession());

  // Partial overview contains the settings button.
  auto* overview_grid =
      ash::OverviewController::Get()->overview_session()->GetGridWithRootWindow(
          window->GetRootWindow());
  ASSERT_TRUE(overview_grid);
  auto* faster_split_view = overview_grid->GetFasterSplitView();
  ASSERT_TRUE(faster_split_view);
  auto* settings_button = faster_split_view->settings_button();
  ASSERT_TRUE(settings_button);

  // Setup navigation observer to wait for the OS Settings page.
  constexpr char kOsSettingsUrl[] =
      "chrome://os-settings/personalization?settingId=1900";
  GURL os_settings(kOsSettingsUrl);
  content::TestNavigationObserver navigation_observer(os_settings);
  navigation_observer.StartWatchingNewWebContents();

  // Click the overview settings button.
  ClickButton(settings_button);

  // Wait for OS Settings to open.
  navigation_observer.Wait();

  // Verify correct OS Settings page is opened.
  Browser* settings_browser = ash::FindSystemWebAppBrowser(
      browser()->profile(), ash::SystemWebAppType::SETTINGS);
  ASSERT_TRUE(settings_browser);
  ASSERT_EQ(os_settings, GetActiveUrl(settings_browser));
}
