// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/constants/ash_features.h"
#include "ash/display/display_move_window_util.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_setup_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/new_window/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

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

// This class observes the `TabStripModelObserver` and reacts with predetermined
// actions to manage tab behavior. In particular, it finalizes tab detachment
// by releasing the left mouse button when a tab strip is removed from a window.
class TabRemoveObserver : public TabStripModelObserver {
 public:
  TabRemoveObserver(Browser* browser, ui::test::EventGenerator* event_generator)
      : browser_(browser), event_generator_(event_generator) {
    browser_->tab_strip_model()->AddObserver(this);
  }
  TabRemoveObserver(const TabRemoveObserver&) = delete;
  TabRemoveObserver& operator=(const TabRemoveObserver&) = delete;
  ~TabRemoveObserver() override {
    browser_->tab_strip_model()->RemoveObserver(this);
  }

  // TabStripModelObserver:
  void OnTabWillBeRemoved(content::WebContents* contents, int index) override {
    // Tab detachment is asynchronous. Release the mouse button after the tab
    // move is done.
    event_generator_->ReleaseLeftButton();
  }

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<ui::test::EventGenerator> event_generator_;
};

}  // namespace

// -----------------------------------------------------------------------------
// FasterSplitScreenBrowserTest:

using FasterSplitScreenBrowserTest = InProcessBrowserTest;

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

  auto* split_view_setup_view = overview_grid->GetSplitViewSetupView();
  ASSERT_TRUE(split_view_setup_view);
  views::Button* settings_button = const_cast<views::Button*>(
      views::AsViewClass<views::Button>(split_view_setup_view->GetViewByID(
          ash::SplitViewSetupView::kSettingsButtonIDForTest)));
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

// -----------------------------------------------------------------------------
// FasterSplitScreenWithOldSettingsBrowserTest:

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
  auto* split_view_setup_view = overview_grid->GetSplitViewSetupView();
  ASSERT_TRUE(split_view_setup_view);
  views::Button* settings_button = const_cast<views::Button*>(
      views::AsViewClass<views::Button>(split_view_setup_view->GetViewByID(
          ash::SplitViewSetupView::kSettingsButtonIDForTest)));
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

// -----------------------------------------------------------------------------
// SnapGroupBrowserTest:

class SnapGroupBrowserTest : public InProcessBrowserTest {
 public:
  SnapGroupBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kForestFeature,
                              ash::features::kSavedDeskUiRevamp},
        /*disabled_features=*/{});
  }
  SnapGroupBrowserTest(const SnapGroupBrowserTest&) = delete;
  SnapGroupBrowserTest& operator=(const SnapGroupBrowserTest&) = delete;
  ~SnapGroupBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that creating a snap group in a rotated display works correctly.
// Regression test for http://335323173.
// Test layout, where Display 2 is rotated such that Files is physically on top
// and Settings on bottom:
// +----------------+----------+
// |                |          |
// |    Display 1   |  Files   |
// |                |          |
// +----------------|----------|
//                  |          |
//                  | Settings |
//                  |          |
//                  +----------+
IN_PROC_BROWSER_TEST_F(SnapGroupBrowserTest, RotatedSnapGroup) {
  auto* display_manager = ash::Shell::Get()->display_manager();
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("0+0-800x600,800+0-1200x900/r");
  display::test::DisplayManagerTestApi display_manager_test(display_manager);

  const auto& displays = display_manager->active_display_list();
  const display::Display display2 = displays[1];
  ASSERT_EQ(display2.id(), display_manager_test.GetSecondaryDisplay().id());
  ASSERT_EQ(chromeos::OrientationType::kPortraitSecondary,
            chromeos::GetDisplayCurrentOrientation(display2));

  // Open Files and Settings app windows.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ash::test::InstallSystemAppsForTesting(profile);

  ash::test::CreateSystemWebApp(profile, ash::SystemWebAppType::FILE_MANAGER);
  aura::Window* w1 =
      BrowserList::GetInstance()->GetLastActive()->window()->GetNativeWindow();
  ash::display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  auto* root2 = ash::Shell::GetAllRootWindows()[1].get();
  ASSERT_EQ(root2, w1->GetRootWindow());

  ash::test::CreateSystemWebApp(profile, ash::SystemWebAppType::SETTINGS);
  aura::Window* w2 =
      BrowserList::GetInstance()->GetLastActive()->window()->GetNativeWindow();
  ash::display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  ASSERT_EQ(root2, w1->GetRootWindow());

  // Snap Files to secondary which is physically on top.
  ash::WindowState* window_state1 = ash::WindowState::Get(w1);
  const ash::WindowSnapWMEvent primary_snap_event(
      ash::WM_EVENT_SNAP_SECONDARY,
      ash::WindowSnapActionSource::kDragWindowToEdgeToSnap);
  window_state1->OnWMEvent(&primary_snap_event);
  ash::WaitForOverviewEntered();
  ASSERT_TRUE(ash::OverviewController::Get()->InOverviewSession());

  // Test the bounds are updated. Rects are also hardcoded for readability.
  // Note `display::Display::work_area()` is still relative to the screen, i.e.
  // not rotated.
  const gfx::Rect work_area2 =
      ash::screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          w1);
  ASSERT_EQ(gfx::Rect(800, 0, 900, 1152), work_area2);
  gfx::Rect top_half, bottom_half;
  work_area2.SplitHorizontally(top_half, bottom_half);
  ASSERT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            ash::WindowState::Get(w1)->GetStateType());
  EXPECT_EQ(top_half, w1->GetBoundsInScreen());

  // Activate `w2` to simulate clicking it from overview.
  wm::ActivateWindow(w2);
  ash::WaitForOverviewExitAnimation();
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            ash::WindowState::Get(w2)->GetStateType());
  EXPECT_TRUE(ash::SnapGroupController::Get()->AreWindowsInSnapGroup(w1, w2));

  // Test the bounds are updated. Rects are also hardcoded for readability.
  const gfx::Rect divider_bounds(
      work_area2.x(),
      work_area2.CenterPoint().y() - ash::kSplitviewDividerShortSideLength / 2,
      work_area2.width(), ash::kSplitviewDividerShortSideLength);
  ASSERT_EQ(gfx::Rect(800, 573, 900, 6), divider_bounds);
  EXPECT_EQ(divider_bounds, ash::SnapGroupController::Get()
                                ->GetSnapGroupForGivenWindow(w1)
                                ->snap_group_divider()
                                ->GetDividerBoundsInScreen(
                                    /*is_dragging=*/false));
  top_half.Subtract(divider_bounds);
  bottom_half.Subtract(divider_bounds);
  ASSERT_EQ(gfx::Rect(800, 0, 900, 573), top_half);
  EXPECT_EQ(top_half, w1->GetBoundsInScreen());
  ASSERT_EQ(gfx::Rect(800, 579, 900, 573), bottom_half);
  EXPECT_EQ(bottom_half, w2->GetBoundsInScreen());
}

// Verify that dragging a tab within a Snap Group window does not break the
// group.
IN_PROC_BROWSER_TEST_F(SnapGroupBrowserTest, DoNotBreakGroupOnTabDragging) {
  aura::Window* window1 = browser()->window()->GetNativeWindow();
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUITabSearchURL), -1, true);
  ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());

  aura::Window* window2 =
      CreateBrowser(browser()->profile())->window()->GetNativeWindow();

  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  ash::SnapTwoTestWindows(window1, window2, /*horizontal=*/true,
                          &event_generator);
  ASSERT_TRUE(
      ash::SnapGroupController::Get()->AreWindowsInSnapGroup(window1, window2));

  TabStrip* tap_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  const auto start_point =
      tap_strip->tab_at(1)->GetBoundsInScreen().CenterPoint();
  const auto end_point =
      tap_strip->tab_at(0)->GetBoundsInScreen().left_center();
  event_generator.MoveMouseTo(start_point);
  event_generator.PressLeftButton();
  event_generator.MoveMouseTo(end_point);
  event_generator.ReleaseLeftButton();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(
      ash::SnapGroupController::Get()->AreWindowsInSnapGroup(window1, window2));
}

// Verify that detaching a tab from a window within a Snap Group doesn't break
// the group.
IN_PROC_BROWSER_TEST_F(SnapGroupBrowserTest, DoNotBreakGroupOnTabDetaching) {
  aura::Window* window1 = browser()->window()->GetNativeWindow();
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUITabSearchURL), -1, true);
  ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());

  aura::Window* window2 =
      CreateBrowser(browser()->profile())->window()->GetNativeWindow();

  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());
  ash::SnapTwoTestWindows(window1, window2, /*horizontal=*/true,
                          &event_generator);
  ASSERT_TRUE(
      ash::SnapGroupController::Get()->AreWindowsInSnapGroup(window1, window2));

  ASSERT_EQ(2u, chrome::GetTotalBrowserCount());

  TabStrip* tap_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  const gfx::Point start_point =
      tap_strip->tab_at(1)->GetBoundsInScreen().CenterPoint();
  const gfx::Point end_point = window2->GetBoundsInScreen().CenterPoint();
  event_generator.MoveMouseTo(start_point);
  event_generator.PressLeftButton();
  TabRemoveObserver observer(browser(), &event_generator);
  event_generator.MoveMouseTo(end_point);

  // Verify that detaching a tab results in a new window being created and that
  // `window1` and `window2` still belong to the Snap Group.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(
      ash::SnapGroupController::Get()->AreWindowsInSnapGroup(window1, window2));
}

// Test that "Save Desk for Later" is not supported when both windows in a Snap
// Group are in incognito mode.
IN_PROC_BROWSER_TEST_F(SnapGroupBrowserTest,
                       SaveDeskForLaterWithTwoIncognitoWindows) {
  auto* desks_controller = ash::DesksController::Get();
  desks_controller->NewDesk(ash::DesksCreationRemovalSource::kButton);
  const auto& desks = desks_controller->desks();
  ASSERT_EQ(2u, desks.size());
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();

  // Explicitly move the default non-incognito browser window to another desk.
  aura::Window* window = browser()->window()->GetNativeWindow();
  desks_controller->MoveWindowFromActiveDeskTo(
      window, desks[1].get(), root_window,
      ash::DesksMoveWindowFromActiveDeskSource::kShortcut);

  // Create a Snap Group with two incognito browser windows.
  Browser* incognito_browser1 = CreateIncognitoBrowser();
  aura::Window* window1 = incognito_browser1->window()->GetNativeWindow();
  Browser* incognito_browser2 = CreateIncognitoBrowser();
  aura::Window* window2 = incognito_browser2->window()->GetNativeWindow();

  ui::test::EventGenerator event_generator(root_window);
  ash::SnapTwoTestWindows(window1, window2, /*horizontal=*/true,
                          &event_generator);
  ash::OverviewController::Get()->StartOverview(
      ash::OverviewStartAction::kOverviewButton);
  ash::WaitForOverviewEntered();
  ASSERT_TRUE(ash::IsInOverviewSession());

  ASSERT_EQ(2u, ash::GetPrimaryRootDesksBarView()->mini_views().size());
  ash::OverviewGrid* overview_grid = ash::GetOverviewGridForRoot(root_window);
  EXPECT_EQ(1u, overview_grid->item_list().size());
  auto* desks_bar_view0 = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view0);
  ash::DeskMiniView* desk_mini_view0 = desks_bar_view0->mini_views()[0];
  ASSERT_TRUE(desk_mini_view0);

  event_generator.MoveMouseTo(
      desk_mini_view0->GetBoundsInScreen().CenterPoint());
  event_generator.ClickRightButton();

  // Activate the desk mini view to enable context menu.
  ash::DeskActionContextMenu* mini_view_menu0 = desk_mini_view0->context_menu();
  ASSERT_TRUE(mini_view_menu0);

  const views::MenuItemView* save_for_later_item =
      ash::DesksTestApi::GetDeskActionContextMenuItem(
          mini_view_menu0,
          ash::DeskActionContextMenu::CommandId::kSaveForLater);
  ASSERT_TRUE(save_for_later_item);

  // Click on the "Save Desk for Later" button in the context menu.
  event_generator.MoveMouseTo(
      save_for_later_item->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  // Verify that the "Save to Desk" feature is not supported as both windows
  // within a Snap Group are in incognito mode.
  EXPECT_TRUE(ash::IsInOverviewSession());
  EXPECT_EQ(1u, overview_grid->item_list().size());
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
}
