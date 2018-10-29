// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/default_frame_header.h"
#include "ash/public/cpp/frame_header.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.mojom.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"  // mash-ok
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/test_multi_user_window_manager.h"
#include "chrome/browser/ui/ash/tablet_mode_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/frame/hosted_app_menu_button.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

// Toggles fullscreen mode and waits for the notification.
void ToggleFullscreenModeAndWait(Browser* browser) {
  FullscreenNotificationObserver waiter;
  chrome::ToggleFullscreenMode(browser);
  waiter.Wait();
}

// Enters fullscreen mode for tab and waits for the notification.
void EnterFullscreenModeForTabAndWait(Browser* browser,
                                      content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter;
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->EnterFullscreenModeForTab(web_contents, GURL());
  waiter.Wait();
}

// Exits fullscreen mode for tab and waits for the notification.
void ExitFullscreenModeForTabAndWait(Browser* browser,
                                     content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter;
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ExitFullscreenModeForTab(web_contents);
  waiter.Wait();
}

// Exits fullscreen mode and waits for the notification.
void ExitFullscreenModeAndWait(BrowserView* browser_view) {
  FullscreenNotificationObserver waiter;
  browser_view->ExitFullscreen();
  waiter.Wait();
}

BrowserNonClientFrameViewAsh* GetFrameViewAsh(BrowserView* browser_view) {
  // We know we're using Ash, so static cast.
  auto* frame_view = static_cast<BrowserNonClientFrameViewAsh*>(
      browser_view->GetWidget()->non_client_view()->frame_view());
  DCHECK(frame_view);
  return frame_view;
}

// Template to be used as a base class for touch-optimized UI parameterized test
// fixtures.
template <class BaseTest>
class TopChromeMdParamTest : public BaseTest,
                             public ::testing::WithParamInterface<bool> {
 public:
  TopChromeMdParamTest() : test_api_(GetParam()) {}
  ~TopChromeMdParamTest() override = default;

 private:
  ui::test::MaterialDesignControllerTestAPI test_api_;

  DISALLOW_COPY_AND_ASSIGN(TopChromeMdParamTest);
};

// A helper class for immersive mode tests.
class ImmersiveModeTester : public ImmersiveModeController::Observer {
 public:
  explicit ImmersiveModeTester(Browser* browser) : browser_(browser) {
    ash::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerAsh*>(
            GetBrowserView()->immersive_mode_controller())
            ->controller())
        .SetupForTest();
    scoped_observer_.Add(GetBrowserView()->immersive_mode_controller());
  }
  ~ImmersiveModeTester() override = default;

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser_);
  }

  // Runs the given command, verifies that a reveal happens and the expected tab
  // is active.
  void RunCommand(int command, int expected_index) {
    reveal_started_ = reveal_ended_ = false;
    browser_->command_controller()->ExecuteCommand(command);
    VerifyTabIndexAfterReveal(expected_index);
  }

  // Verifies a reveal has happened and the expected tab is active.
  void VerifyTabIndexAfterReveal(int expected_index) {
    if (!reveal_ended_) {
      reveal_loop_ = std::make_unique<base::RunLoop>();
      reveal_loop_->Run();
    }
    EXPECT_TRUE(reveal_ended_);
    EXPECT_EQ(expected_index, browser_->tab_strip_model()->active_index());
  }

  // Waits for the immersive fullscreen to end (or returns immediately if
  // immersive fullscreen already ended).
  void WaitForFullscreenToExit() {
    if (GetBrowserView()->immersive_mode_controller()->IsEnabled()) {
      fullscreen_loop_ = std::make_unique<base::RunLoop>();
      fullscreen_loop_->Run();
    }
    EXPECT_FALSE(GetBrowserView()->immersive_mode_controller()->IsEnabled());
  }

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override {
    EXPECT_FALSE(reveal_started_);
    EXPECT_FALSE(reveal_ended_);
    reveal_started_ = true;
    EXPECT_TRUE(GetBrowserView()->immersive_mode_controller()->IsRevealed());
  }

  void OnImmersiveRevealEnded() override {
    EXPECT_TRUE(reveal_started_);
    EXPECT_FALSE(reveal_ended_);
    reveal_started_ = false;
    reveal_ended_ = true;
    EXPECT_FALSE(GetBrowserView()->immersive_mode_controller()->IsRevealed());
    if (reveal_loop_ && reveal_loop_->running())
      reveal_loop_->Quit();
  }

  void OnImmersiveModeControllerDestroyed() override {
    scoped_observer_.RemoveAll();
  }

  void OnImmersiveFullscreenExited() override {
    if (fullscreen_loop_ && fullscreen_loop_->running())
      fullscreen_loop_->Quit();
  }

 private:
  Browser* browser_ = nullptr;
  ScopedObserver<ImmersiveModeController, ImmersiveModeController::Observer>
      scoped_observer_{this};
  bool reveal_started_ = false;
  bool reveal_ended_ = false;
  std::unique_ptr<base::RunLoop> reveal_loop_;
  std::unique_ptr<base::RunLoop> fullscreen_loop_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeTester);
};

}  // namespace

using views::Widget;

using BrowserNonClientFrameViewAshTest =
    TopChromeMdParamTest<InProcessBrowserTest>;

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, NonClientHitTest) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  // Click on the top edge of a restored window hits the top edge resize handle.
  const int kWindowWidth = 300;
  const int kWindowHeight = 290;
  widget->SetBounds(gfx::Rect(10, 10, kWindowWidth, kWindowHeight));
  gfx::Point top_edge(kWindowWidth / 2, 0);
  EXPECT_EQ(HTTOP, frame_view->NonClientHitTest(top_edge));

  // Click just below the resize handle hits the caption.
  gfx::Point below_resize(kWindowWidth / 2, ash::kResizeInsideBoundsSize);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(below_resize));

  // Click in the top edge of a maximized window now hits the client area,
  // because we want it to fall through to the tab strip and select a tab.
  widget->Maximize();
  int expected_value = HTCLIENT;
  EXPECT_EQ(expected_value, frame_view->NonClientHitTest(top_edge));
}

// Test that the frame view does not do any painting in non-immersive
// fullscreen.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       NonImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());

  // No painting should occur in non-immersive fullscreen. (We enter into tab
  // fullscreen here because tab fullscreen is non-immersive even on ChromeOS).
  {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously.
    std::unique_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents, GURL());
    waiter->Wait();
  }
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(frame_view->ShouldPaint());

  // The client view abuts top of the window.
  EXPECT_EQ(0, frame_view->GetBoundsForClientView().y());

  // The frame should be painted again when fullscreen is exited and the caption
  // buttons should be visible.
  ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(frame_view->ShouldPaint());
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, ImmersiveFullscreen) {
  aura::test::EnvTestHelper().SetAlwaysUseLastMouseLocation(true);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  ASSERT_EQ(ImmersiveModeController::Type::ASH,
            immersive_mode_controller->type());

  ash::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerAsh*>(immersive_mode_controller)
          ->controller())
      .SetupForTest();

  // Immersive fullscreen starts disabled.
  ASSERT_FALSE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(
      0, frame_view->GetBoundsForTabStrip(browser_view->tabstrip()).bottom());

  // Enter both browser fullscreen and tab fullscreen. Entering browser
  // fullscreen should enable immersive fullscreen.
  ToggleFullscreenModeAndWait(browser());
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());

  // An immersive reveal shows the buttons and the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());

  // End the reveal. When in both immersive browser fullscreen and tab
  // fullscreen.
  revealed_lock.reset();
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  EXPECT_FALSE(frame_view->ShouldPaint());
  EXPECT_EQ(
      0, frame_view->GetBoundsForTabStrip(browser_view->tabstrip()).bottom());

  // Repeat test but without tab fullscreen.
  ExitFullscreenModeForTabAndWait(browser(), web_contents);

  // Immersive reveal should have same behavior as before.
  revealed_lock.reset(immersive_mode_controller->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(
      0, frame_view->GetBoundsForTabStrip(browser_view->tabstrip()).bottom());

  // Ending the reveal. Immersive browser should have the same behavior as full
  // screen, i.e., having an origin of (0,0).
  revealed_lock.reset();
  EXPECT_FALSE(frame_view->ShouldPaint());
  EXPECT_EQ(
      0, frame_view->GetBoundsForTabStrip(browser_view->tabstrip()).bottom());

  // Exiting immersive fullscreen should make the caption buttons and the frame
  // visible again.
  ExitFullscreenModeAndWait(browser_view);
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(
      0, frame_view->GetBoundsForTabStrip(browser_view->tabstrip()).bottom());
}

// Tests that Avatar icon should show on the top left corner of the teleported
// browser window on ChromeOS.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       AvatarDisplayOnTeleportedWindow) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);
  aura::Window* window = browser()->window()->GetNativeWindow();

  EXPECT_FALSE(MultiUserWindowManager::ShouldShowAvatar(window));
  EXPECT_FALSE(frame_view->profile_indicator_icon_);

  const AccountId account_id1 =
      multi_user_util::GetAccountIdFromProfile(browser()->profile());
  TestMultiUserWindowManager* manager =
      new TestMultiUserWindowManager(browser(), account_id1);

  // Teleport the window to another desktop.
  const AccountId account_id2(AccountId::FromUserEmail("user2"));
  manager->ShowWindowForUser(window, account_id2);
  EXPECT_TRUE(MultiUserWindowManager::ShouldShowAvatar(window));
  EXPECT_TRUE(frame_view->profile_indicator_icon_);

  // Teleport the window back to owner desktop.
  manager->ShowWindowForUser(window, account_id1);
  EXPECT_FALSE(MultiUserWindowManager::ShouldShowAvatar(window));
  EXPECT_FALSE(frame_view->profile_indicator_icon_);
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       IncognitoMarkedAsAssistantBlocked) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(incognito_browser->window()->GetNativeWindow()->GetProperty(
      ash::kBlockedForAssistantSnapshotKey));
}

// Tests that FrameCaptionButtonContainer has been relaid out in response to
// tablet mode being toggled.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       ToggleTabletModeRelayout) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  const gfx::Rect initial = frame_view->caption_button_container_->bounds();
  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(true));
  ash::FrameCaptionButtonContainerView::TestApi test(
      frame_view->caption_button_container_);
  test.EndAnimations();
  const gfx::Rect during_maximize =
      frame_view->caption_button_container_->bounds();
  EXPECT_GT(initial.width(), during_maximize.width());
  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(false));
  test.EndAnimations();
  const gfx::Rect after_restore =
      frame_view->caption_button_container_->bounds();
  EXPECT_EQ(initial.origin(), after_restore.origin());
  EXPECT_EQ(initial.width(), after_restore.width());

  // Switching from non-tablet to tablet mode will increase the height of the
  // top frame and toolbar if the MD mode is set to "Dynamic Refresh".
  EXPECT_GE(initial.height(), after_restore.height());
}

// Tests that browser frame minimum size constraint is updated in response to
// browser view layout.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       FrameMinSizeIsUpdated) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  BookmarkBarView* bookmark_bar = browser_view->GetBookmarkBarView();
  EXPECT_FALSE(bookmark_bar->visible());
  const int min_height_no_bookmarks = frame_view->GetMinimumSize().height();

  // Setting non-zero bookmark bar preferred size forces it to be visible and
  // triggers BrowserView layout update.
  bookmark_bar->SetPreferredSize(gfx::Size(50, 5));
  EXPECT_TRUE(bookmark_bar->visible());

  // Minimum window size should grow with the bookmark bar shown.
  // kMinimumSize window property should get updated.
  aura::Window* window = browser()->window()->GetNativeWindow();
  const gfx::Size* min_window_size =
      window->GetProperty(aura::client::kMinimumSize);
  ASSERT_NE(nullptr, min_window_size);
  EXPECT_GT(min_window_size->height(), min_height_no_bookmarks);
  EXPECT_EQ(*min_window_size, frame_view->GetMinimumSize());
}

// This is a regression test that session restore minimized browser should
// re-layout the header (https://crbug.com/827444).
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       RestoreMinimizedBrowserUpdatesCaption) {
  // Enable session service.
  SessionStartupPref pref(SessionStartupPref::LAST);
  Profile* profile = browser()->profile();
  SessionStartupPref::SetStartupPref(profile, pref);

  SessionServiceTestHelper helper(
      SessionServiceFactory::GetForProfile(profile));
  helper.SetForceBrowserNotAliveWithNoWindows(true);
  helper.ReleaseService();

  // Do not exit from test when last browser is closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                             KeepAliveRestartOption::DISABLED);

  // Quit and restore.
  browser()->window()->Minimize();
  CloseBrowserSynchronously(browser());

  chrome::NewEmptyWindow(profile);
  ui_test_utils::BrowserAddedObserver window_observer;
  SessionRestoreTestHelper restore_observer;

  Browser* new_browser = window_observer.WaitForSingleNewBrowser();

  // Check that a layout occurs.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(new_browser);
  Widget* widget = browser_view->GetWidget();
  restore_observer.Wait();

  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  ash::FrameCaptionButtonContainerView::TestApi test(
      frame_view->caption_button_container_);
  EXPECT_TRUE(test.size_button()->icon_definition_for_test());
}

namespace {

class ImmersiveModeBrowserViewTest
    : public TopChromeMdParamTest<InProcessBrowserTest> {
 public:
  ImmersiveModeBrowserViewTest() = default;
  ~ImmersiveModeBrowserViewTest() override = default;

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    aura::test::EnvTestHelper().SetAlwaysUseLastMouseLocation(true);
    BrowserView::SetDisableRevealerDelayForTesting(true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeBrowserViewTest);
};

}  // namespace

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB when the browser is in immersive fullscreen mode.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TabNavigationAcceleratorsFullscreenBrowser) {
  ImmersiveModeTester tester(browser());
  // Make sure that the focus is on the webcontents rather than on the omnibox,
  // because if the focus is on the omnibox, the tab strip will remain revealed
  // in the immersive fullscreen mode and will interfere with this test waiting
  // for the revealer to be dismissed.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Create three more tabs plus the existing one that browser tests start with.
  const GURL about_blank(url::kAboutBlankURL);
  AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED);
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED);
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED);
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  // Wait for the end of the initial reveal which results from adding the new
  // tabs and changing the focused tab.
  tester.VerifyTabIndexAfterReveal(0);

  // Groups the browser command ID and its corresponding active tab index that
  // will result when the command is executed in this test.
  struct TestData {
    int command;
    int expected_index;
  };
  constexpr TestData test_data[] = {{IDC_SELECT_LAST_TAB, 3},
                                    {IDC_SELECT_TAB_0, 0},
                                    {IDC_SELECT_NEXT_TAB, 1},
                                    {IDC_SELECT_PREVIOUS_TAB, 0}};
  for (const auto& datum : test_data)
    tester.RunCommand(datum.command, datum.expected_index);
}

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TestCaptionButtonsReceiveEventsInBrowserImmersiveMode) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ash::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller())
          ->controller())
      .SetupForTest();

  // Make sure that the focus is on the webcontents rather than on the omnibox,
  // because if the focus is on the omnibox, the tab strip will remain revealed
  // in the immersive fullscreen mode and will interfere with this test waiting
  // for the revealer to be dismissed.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());

  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsRevealed());

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      browser_view->immersive_mode_controller()->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());

  ImmersiveModeTester tester(browser());

  // Clicking the "restore" caption button should exit the immersive mode.
  aura::Window* window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  gfx::Size button_size =
      ash::GetAshLayoutSize(ash::AshLayoutSize::kBrowserCaptionMaximized);
  gfx::Point point_in_restore_button(
      window->GetBoundsInRootWindow().top_right());
  point_in_restore_button.Offset(-2 * button_size.width(),
                                 button_size.height() / 2);

  event_generator.MoveMouseTo(point_in_restore_button);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());
  event_generator.ClickLeftButton();
  tester.WaitForFullscreenToExit();

  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TestCaptionButtonsReceiveEventsInAppImmersiveMode) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(0, 0, 300, 300),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = new Browser(params);
  ASSERT_TRUE(browser->is_app());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  ash::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller())
          ->controller())
      .SetupForTest();

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser_view->IsTabStripVisible());

  EXPECT_TRUE(browser->window()->IsFullscreen());
  EXPECT_FALSE(browser->window()->IsMaximized());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsRevealed());

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      browser_view->immersive_mode_controller()->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());

  ImmersiveModeTester tester(browser);
  AddBlankTabAndShow(browser);

  // Clicking the "restore" caption button should exit the immersive mode.
  aura::Window* window = browser->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
  gfx::Size button_size =
      ash::GetAshLayoutSize(ash::AshLayoutSize::kBrowserCaptionMaximized);
  gfx::Point point_in_restore_button(
      window->GetBoundsInRootWindow().top_right());
  point_in_restore_button.Offset(-2 * button_size.width(),
                                 button_size.height() / 2);

  event_generator.MoveMouseTo(point_in_restore_button);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());
  event_generator.ClickLeftButton();
  tester.WaitForFullscreenToExit();

  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser->window()->IsFullscreen());
}

namespace {

class HostedAppNonClientFrameViewAshTest
    : public TopChromeMdParamTest<BrowserActionsBarBrowserTest> {
 public:
  HostedAppNonClientFrameViewAshTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~HostedAppNonClientFrameViewAshTest() override = default;

  GURL GetAppURL() {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }
  static SkColor GetThemeColor() { return SK_ColorBLUE; }

  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  ash::DefaultFrameHeader* frame_header_ = nullptr;
  HostedAppButtonContainer* hosted_app_button_container_ = nullptr;
  const std::vector<ContentSettingImageView*>* content_setting_views_ = nullptr;
  BrowserActionsContainer* browser_actions_container_ = nullptr;
  views::MenuButton* app_menu_button_ = nullptr;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TopChromeMdParamTest<BrowserActionsBarBrowserTest>::SetUpCommandLine(
        command_line);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    TopChromeMdParamTest<
        BrowserActionsBarBrowserTest>::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
    TopChromeMdParamTest<
        BrowserActionsBarBrowserTest>::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    TopChromeMdParamTest<BrowserActionsBarBrowserTest>::SetUpOnMainThread();

    scoped_feature_list_.InitAndEnableFeature(features::kDesktopPWAWindowing);
    HostedAppButtonContainer::DisableAnimationForTesting();

    // Start secure local server.
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // |SetUpHostedApp()| must be called after |SetUpOnMainThread()| to make sure
  // the Network Service process has been setup properly.
  void SetUpHostedApp() {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GetAppURL();
    web_app_info.scope = GetAppURL().GetWithoutFilename();
    web_app_info.theme_color = GetThemeColor();

    const extensions::Extension* app = InstallBookmarkApp(web_app_info);
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = LaunchAppBrowser(app);
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view_);
    frame_header_ =
        static_cast<ash::DefaultFrameHeader*>(frame_view->frame_header_.get());

    hosted_app_button_container_ =
        frame_view->hosted_app_button_container_for_testing();
    DCHECK(hosted_app_button_container_);
    DCHECK(hosted_app_button_container_->visible());

    content_setting_views_ =
        &hosted_app_button_container_->GetContentSettingViewsForTesting();
    browser_actions_container_ =
        hosted_app_button_container_->browser_actions_container_;
    app_menu_button_ = hosted_app_button_container_->app_menu_button_;
  }

  AppMenu* GetAppMenu() {
    return hosted_app_button_container_->app_menu_button_
        ->app_menu_for_testing();
  }

  SkColor GetActiveColor() {
    return hosted_app_button_container_->active_color_;
  }

  bool GetPaintingAsActive() {
    return hosted_app_button_container_->paint_as_active_;
  }

  PageActionIconView* GetPageActionIcon(PageActionIconType type) {
    return browser_view_->toolbar_button_provider()
        ->GetPageActionIconContainerView()
        ->GetPageActionIconView(type);
  }

  ContentSettingImageView* GrantGeolocationPermission() {
    content::RenderFrameHost* frame =
        app_browser_->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    TabSpecificContentSettings* content_settings =
        TabSpecificContentSettings::GetForFrame(frame->GetProcess()->GetID(),
                                                frame->GetRoutingID());
    content_settings->OnGeolocationPermissionSet(GetAppURL().GetOrigin(), true);

    return *std::find_if(
        content_setting_views_->begin(), content_setting_views_->end(),
        [](auto v) {
          return ContentSettingImageModel::ImageType::GEOLOCATION ==
                 v->GetTypeForTesting();
        });
  }

  void SimulateClickOnView(views::View* view) {
    const gfx::Point point;
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseEvent(&event);
    ui::MouseEvent event_rel(ui::ET_MOUSE_RELEASED, point, point,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseEvent(&event_rel);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // For mocking a secure site.
  net::EmbeddedTestServer https_server_;
  ChromeMockCertVerifier cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppNonClientFrameViewAshTest);
};

}  // namespace

// Tests that the page info dialog doesn't anchor in a way that puts it outside
// of hosted app windows. This is important as some platforms don't support
// bubble anchor adjustment (see |BubbleDialogDelegateView::CreateBubble()|).
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       PageInfoBubblePosition) {
  SetUpHostedApp();
  // Resize app window to only take up the left half of the screen.
  views::Widget* widget = browser_view_->GetWidget();
  gfx::Size screen_size =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget->GetNativeWindow())
          .work_area_size();
  widget->SetBounds(
      gfx::Rect(0, 0, screen_size.width() / 2, screen_size.height()));

  // Show page info dialog (currently PWAs use page info in place of an actual
  // app info dialog).
  chrome::ExecuteCommand(app_browser_, IDC_HOSTED_APP_MENU_APP_INFO);

  // Check the bubble anchors inside the main app window even if there's space
  // available outside the main app window.
  gfx::Rect page_info_bounds = PageInfoBubbleViewBase::GetPageInfoBubble()
                                   ->GetWidget()
                                   ->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget->GetWindowBoundsInScreen().Contains(page_info_bounds));
}

IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, FocusableViews) {
  SetUpHostedApp();
  EXPECT_TRUE(browser_view_->contents_web_view()->HasFocus());
  browser_view_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(app_menu_button_->HasFocus());
  browser_view_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(browser_view_->contents_web_view()->HasFocus());
}

// Tests that a web app's theme color is set.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, ThemeColor) {
  SetUpHostedApp();
  aura::Window* window = browser_view_->GetWidget()->GetNativeWindow();
  EXPECT_EQ(GetThemeColor(), window->GetProperty(ash::kFrameActiveColorKey));
  EXPECT_EQ(GetThemeColor(), window->GetProperty(ash::kFrameInactiveColorKey));
  EXPECT_EQ(SK_ColorWHITE, GetActiveColor());
}

// Make sure that for hosted apps, the height of the frame doesn't exceed the
// height of the caption buttons.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, FrameSize) {
  SetUpHostedApp();
  const int inset = GetFrameViewAsh(browser_view_)->GetTopInset(false);
  EXPECT_EQ(inset,
            GetAshLayoutSize(ash::AshLayoutSize::kNonBrowserCaption).height());
  EXPECT_GE(inset, app_menu_button_->size().height());
  EXPECT_GE(inset, hosted_app_button_container_->size().height());
}

// Test that the HostedAppButtonContainer is the designated toolbar button
// provider in this window configuration.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       ToolbarButtonProvider) {
  SetUpHostedApp();
  EXPECT_EQ(browser_view_->toolbar_button_provider(),
            hosted_app_button_container_);
}

// Test that the zoom icon appears in the title bar for hosted app windows.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, ZoomIcon) {
  SetUpHostedApp();
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  PageActionIconView* zoom_icon = GetPageActionIcon(PageActionIconType::kZoom);

  EXPECT_TRUE(zoom_icon);
  EXPECT_FALSE(zoom_icon->visible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(zoom_icon->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());
}

// Test that the find icon appears in the title bar for hosted app windows.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, FindIcon) {
  SetUpHostedApp();
  PageActionIconView* find_icon = GetPageActionIcon(PageActionIconType::kFind);

  EXPECT_TRUE(find_icon);
  EXPECT_FALSE(find_icon->visible());

  chrome::Find(app_browser_);

  EXPECT_TRUE(find_icon->visible());
}

// Tests that the focus toolbar command focuses the app menu button in web app
// windows.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       BrowserCommandFocusToolbarAppMenu) {
  SetUpHostedApp();
  EXPECT_FALSE(app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_TOOLBAR);
  EXPECT_TRUE(app_menu_button_->HasFocus());
}

// Tests that the focus toolbar command focuses content settings icons before
// the app menu button when present in web app windows.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       BrowserCommandFocusToolbarGeolocation) {
  SetUpHostedApp();
  ContentSettingImageView* geolocation_icon = GrantGeolocationPermission();

  EXPECT_FALSE(app_menu_button_->HasFocus());
  EXPECT_FALSE(geolocation_icon->HasFocus());

  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_TOOLBAR);

  EXPECT_FALSE(app_menu_button_->HasFocus());
  EXPECT_TRUE(geolocation_icon->HasFocus());
}

// Tests that the show app menu command opens the app menu for web app windows.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       BrowserCommandShowAppMenu) {
  SetUpHostedApp();
  EXPECT_EQ(nullptr, GetAppMenu());
  chrome::ExecuteCommand(app_browser_, IDC_SHOW_APP_MENU);
  EXPECT_NE(nullptr, GetAppMenu());
}

// Tests that the focus next pane command focuses the app menu for web app
// windows.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       BrowserCommandFocusNextPane) {
  SetUpHostedApp();
  EXPECT_FALSE(app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_NEXT_PANE);
  EXPECT_TRUE(app_menu_button_->HasFocus());
}

// Tests that the focus previous pane command focuses the app menu for web app
// windows.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       BrowserCommandFocusPreviousPane) {
  SetUpHostedApp();
  EXPECT_FALSE(app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_PREVIOUS_PANE);
  EXPECT_TRUE(app_menu_button_->HasFocus());
}

// Tests that a web app's content settings icons can be interacted with.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       ContentSettingIcons) {
  SetUpHostedApp();
  for (auto* view : *content_setting_views_)
    EXPECT_FALSE(view->visible());

  ContentSettingImageView* geolocation_icon = GrantGeolocationPermission();

  for (auto* view : *content_setting_views_) {
    bool is_geolocation_icon = view == geolocation_icon;
    EXPECT_EQ(is_geolocation_icon, view->visible());
  }

  // Press the geolocation button.
  base::HistogramTester histograms;
  geolocation_icon->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
  geolocation_icon->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE));

  histograms.ExpectBucketCount(
      "HostedAppFrame.ContentSettings.ImagePressed",
      static_cast<int>(ContentSettingImageModel::ImageType::GEOLOCATION), 1);
  histograms.ExpectBucketCount(
      "ContentSettings.ImagePressed",
      static_cast<int>(ContentSettingImageModel::ImageType::GEOLOCATION), 1);
}

// Tests that a web app's browser action icons can be interacted with.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, BrowserActions) {
  SetUpHostedApp();
  // Even though 2 are visible in the browser, no extension actions should show.
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions_container_->toolbar_actions_bar();
  LoadExtensions();
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(0u, browser_actions_container_->VisibleBrowserActions());

  // Show the menu.
  SimulateClickOnView(app_menu_button_);

  // All extension actions should always be showing in the menu.
  EXPECT_EQ(3u, GetAppMenu()
                    ->extension_toolbar_for_testing()
                    ->container_for_testing()
                    ->VisibleBrowserActions());

  // Popping out an extension makes its action show in the bar.
  toolbar_actions_bar->PopOutAction(toolbar_actions_bar->GetActions()[2], false,
                                    base::DoNothing());
  EXPECT_EQ(1u, browser_actions_container_->VisibleBrowserActions());
}

// Regression test for https://crbug.com/839955
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest,
                       ActiveStateOfButtonMatchesWidget) {
  SetUpHostedApp();
  ash::FrameCaptionButtonContainerView::TestApi test(
      GetFrameViewAsh(browser_view_)->caption_button_container_);
  EXPECT_TRUE(test.size_button()->paint_as_active());
  EXPECT_TRUE(GetPaintingAsActive());

  browser_view_->GetWidget()->Deactivate();
  EXPECT_FALSE(test.size_button()->paint_as_active());
  EXPECT_FALSE(GetPaintingAsActive());
}

namespace {

class BrowserNonClientFrameViewAshBackButtonTest
    : public TopChromeMdParamTest<InProcessBrowserTest> {
 public:
  BrowserNonClientFrameViewAshBackButtonTest() = default;
  ~BrowserNonClientFrameViewAshBackButtonTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kAshEnableV1AppBackButton);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAshBackButtonTest);
};

}  // namespace

// Test if the V1 apps' frame has a back button.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshBackButtonTest,
                       V1BackButton) {
  // Normal browser windows don't have a frame back button.
  BrowserNonClientFrameViewAsh* frame_view =
      GetFrameViewAsh(BrowserView::GetBrowserViewForBrowser(browser()));
  EXPECT_FALSE(frame_view->back_button_);

  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* app_browser = new Browser(params);
  AddBlankTabAndShow(app_browser);

  BrowserNonClientFrameViewAsh* app_frame_view =
      GetFrameViewAsh(BrowserView::GetBrowserViewForBrowser(app_browser));
  ASSERT_TRUE(app_frame_view->back_button_);
  EXPECT_TRUE(app_frame_view->back_button_->visible());
  // The back button should be disabled initially.
  EXPECT_FALSE(app_frame_view->back_button_->enabled());

  // Nagivate to a page. The back button should now be enabled.
  const GURL kAppStartURL("http://example.org/");
  NavigateParams nav_params(app_browser, kAppStartURL,
                            ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&nav_params);
  EXPECT_TRUE(app_frame_view->back_button_->enabled());

  // Go back to the blank. The back button should be disabled again.
  chrome::GoBack(app_browser, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(app_frame_view->back_button_->enabled());
}

// Test the normal type browser's kTopViewInset is always 0.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, TopViewInset) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  aura::Window* window = browser()->window()->GetNativeWindow();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset should be 0 when in immersive mode.
  ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // An immersive reveal shows the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // End the reveal and exit immersive mode.
  // The kTopViewInset should be 0 when immersive mode is exited.
  revealed_lock.reset();
  ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));
}

// Disabled due to high flake rate; https://crbug.com/818170.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       DISABLED_HeaderVisibilityInOverviewAndSplitview) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      ws::mojom::kResizeBehaviorCanMaximize |
          ws::mojom::kResizeBehaviorCanResize);

  // Test that the header is invisible for the browser window in overview mode
  // and visible when not in overview mode.
  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);
  EXPECT_FALSE(frame_view->caption_button_container_->visible());
  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey,
                                            false);
  EXPECT_TRUE(frame_view->caption_button_container_->visible());

  // Create another browser window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser2 = new Browser(params);
  AddBlankTabAndShow(browser2);
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  Widget* widget2 = browser_view2->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view2 =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget2->non_client_view()->frame_view());
  widget2->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      ws::mojom::kResizeBehaviorCanMaximize |
          ws::mojom::kResizeBehaviorCanResize);

  // Test that when one browser window is snapped, the header is visible for the
  // snapped browser window, but invisible for the browser window still in
  // overview mode.
  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(true));
  ash::Shell* shell = ash::Shell::Get();
  ash::SplitViewController* split_view_controller =
      shell->split_view_controller();
  split_view_controller->BindRequest(
      mojo::MakeRequest(&frame_view->split_view_controller_));
  split_view_controller->BindRequest(
      mojo::MakeRequest(&frame_view2->split_view_controller_));
  split_view_controller->AddObserver(
      frame_view->CreateInterfacePtrForTesting());
  split_view_controller->AddObserver(
      frame_view2->CreateInterfacePtrForTesting());
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();

  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);
  frame_view2->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey,
                                             true);
  split_view_controller->SnapWindow(widget->GetNativeWindow(),
                                    ash::SplitViewController::LEFT);
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view2->caption_button_container_->visible());

  // When both browser windows are snapped, the headers are both visible.
  split_view_controller->SnapWindow(widget2->GetNativeWindow(),
                                    ash::SplitViewController::RIGHT);
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_TRUE(frame_view2->caption_button_container_->visible());

  // Toggle overview mode while splitview mode is active. Test that the header
  // is visible for the snapped browser window but not for the other browser
  // window in overview mode.
  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey,
                                            false);
  frame_view2->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey,
                                             true);
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view2->caption_button_container_->visible());
}

// Regression test for https://crbug.com/879851.
// Tests that we don't accidentally change the color of app frame title bars.
// Update expectation if change is intentional.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, AppFrameColor) {
  browser()->window()->Close();

  // Open a new app window.
  Browser* app_browser = new Browser(Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true /* user_gesture */));
  aura::Window* window = app_browser->window()->GetNativeWindow();
  window->Show();

  SkColor active_frame_color = window->GetProperty(ash::kFrameActiveColorKey);
  EXPECT_EQ(active_frame_color, SkColorSetRGB(253, 254, 255))
      << "RGB: " << SkColorGetR(active_frame_color) << ", "
      << SkColorGetG(active_frame_color) << ", "
      << SkColorGetB(active_frame_color);
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       ImmersiveModeTopViewInset) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = new Browser(params);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  aura::Window* window = browser->window()->GetNativeWindow();
  window->Show();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_LT(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset should be 0 when in immersive mode.
  ToggleFullscreenModeAndWait(browser);
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // An immersive reveal shows the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // End the reveal and exit immersive mode.
  // The kTopViewInset should be larger than 0 again when immersive mode is
  // exited.
  revealed_lock.reset();
  ToggleFullscreenModeAndWait(browser);
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_LT(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset is the same as in overview mode.
  const int inset_normal = window->GetProperty(aura::client::kTopViewInset);
  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);
  const int inset_in_overview_mode =
      window->GetProperty(aura::client::kTopViewInset);
  EXPECT_EQ(inset_normal, inset_in_overview_mode);
}

namespace {

class HomeLauncherBrowserNonClientFrameViewAshTest
    : public TopChromeMdParamTest<InProcessBrowserTest> {
 public:
  HomeLauncherBrowserNonClientFrameViewAshTest() = default;
  ~HomeLauncherBrowserNonClientFrameViewAshTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    TopChromeMdParamTest<InProcessBrowserTest>::SetUpDefaultCommandLine(
        command_line);

    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        app_list_features::kEnableHomeLauncher);
    TopChromeMdParamTest<InProcessBrowserTest>::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HomeLauncherBrowserNonClientFrameViewAshTest);
};

class NonHomeLauncherBrowserNonClientFrameViewAshTest
    : public TopChromeMdParamTest<InProcessBrowserTest> {
 public:
  NonHomeLauncherBrowserNonClientFrameViewAshTest() = default;
  ~NonHomeLauncherBrowserNonClientFrameViewAshTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    TopChromeMdParamTest<InProcessBrowserTest>::SetUpDefaultCommandLine(
        command_line);

    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        app_list_features::kEnableHomeLauncher);
    TopChromeMdParamTest<InProcessBrowserTest>::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NonHomeLauncherBrowserNonClientFrameViewAshTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(HomeLauncherBrowserNonClientFrameViewAshTest,
                       TabletModeBrowserCaptionButtonVisibility) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(true));
  EXPECT_FALSE(frame_view->caption_button_container_->visible());

  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);
  EXPECT_FALSE(frame_view->caption_button_container_->visible());
  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey,
                                            false);
  EXPECT_FALSE(frame_view->caption_button_container_->visible());

  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(false));
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
}

IN_PROC_BROWSER_TEST_P(HomeLauncherBrowserNonClientFrameViewAshTest,
                       TabletModeAppCaptionButtonVisibility) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = new Browser(params);
  ASSERT_TRUE(browser->is_app());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);
  EXPECT_TRUE(frame_view->caption_button_container_->visible());

  // Tablet mode doesn't affect app's caption button's visibility.
  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(true));
  EXPECT_TRUE(frame_view->caption_button_container_->visible());

  // However, overview mode does.
  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);
  EXPECT_FALSE(frame_view->caption_button_container_->visible());
  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey,
                                            false);
  EXPECT_TRUE(frame_view->caption_button_container_->visible());

  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(false));
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
}

// Tests that the header of a snapped browser window in splitview mode uses
// the same header height of a maximized window. The test will fail when home
// launcher is enabled, because caption button container is made invisible
// intentionally.
IN_PROC_BROWSER_TEST_P(NonHomeLauncherBrowserNonClientFrameViewAshTest,
                       HeaderHeightForSnappedBrowserInSplitView) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      ws::mojom::kResizeBehaviorCanMaximize |
          ws::mojom::kResizeBehaviorCanResize);

  const int restored_height = frame_view->frame_header_->GetHeaderHeight();

  // Setting the tablet mode must be done before calculating the expected height
  // since the height may change depending on the tablet mode when in the
  // dynamic refresh MD mode.
  ASSERT_NO_FATAL_FAILURE(test::SetAndWaitForTabletMode(true));

  // Maximize the widget and store its frame header height.
  widget->Maximize();
  const int expected_height = frame_view->frame_header_->GetHeaderHeight();
  EXPECT_NE(expected_height, restored_height);
  widget->Restore();
  // Restored in tablet mode has the same header height as maximized in tablet
  // mode.
  EXPECT_EQ(expected_height, frame_view->frame_header_->GetHeaderHeight());

  frame_view->GetFrameWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);

  if (features::IsUsingWindowService()) {
    ash::mojom::ShellTestApiPtr shell_test_api;
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ash::mojom::kServiceName, &shell_test_api);
    base::RunLoop run_loop;
    shell_test_api->SnapWindowInSplitView(content::mojom::kBrowserServiceName,
                                          frame_view->GetServerWindowId(),
                                          run_loop.QuitClosure());
    run_loop.Run();
  } else {
    ash::Shell* shell = ash::Shell::Get();
    ash::SplitViewController* split_view_controller =
        shell->split_view_controller();
    split_view_controller->BindRequest(
        mojo::MakeRequest(&frame_view->split_view_controller_));
    split_view_controller->AddObserver(
        frame_view->CreateInterfacePtrForTesting());
    frame_view->split_view_controller_.FlushForTesting();

    split_view_controller->SnapWindow(widget->GetNativeWindow(),
                                      ash::SplitViewController::LEFT);
    frame_view->split_view_controller_.FlushForTesting();
  }

  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_EQ(expected_height, frame_view->frame_header_->GetHeaderHeight());
}

#define INSTANTIATE_TEST_CASE(name) \
  INSTANTIATE_TEST_CASE_P(, name, ::testing::Values(false, true))

INSTANTIATE_TEST_CASE(BrowserNonClientFrameViewAshTest);
INSTANTIATE_TEST_CASE(ImmersiveModeBrowserViewTest);
INSTANTIATE_TEST_CASE(HostedAppNonClientFrameViewAshTest);
INSTANTIATE_TEST_CASE(BrowserNonClientFrameViewAshBackButtonTest);
INSTANTIATE_TEST_CASE(HomeLauncherBrowserNonClientFrameViewAshTest);
INSTANTIATE_TEST_CASE(NonHomeLauncherBrowserNonClientFrameViewAshTest);
