// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/default_frame_header.h"
#include "ash/public/cpp/frame_header.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/split_view_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/multi_user/test_multi_user_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_menu_button.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/chromeos_ui_constants.h"
#include "components/account_id/account_id.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"

namespace {

// Toggles fullscreen mode and waits for the notification.
void ToggleFullscreenModeAndWait(Browser* browser) {
  FullscreenNotificationObserver waiter(browser);
  chrome::ToggleFullscreenMode(browser);
  waiter.Wait();
}

// Enters fullscreen mode for tab and waits for the notification.
void EnterFullscreenModeForTabAndWait(Browser* browser,
                                      content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter(browser);
  static_cast<content::WebContentsDelegate*>(browser)
      ->EnterFullscreenModeForTab(web_contents->GetMainFrame(), {});
  waiter.Wait();
}

// Exits fullscreen mode for tab and waits for the notification.
void ExitFullscreenModeForTabAndWait(Browser* browser,
                                     content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter(browser);
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ExitFullscreenModeForTab(web_contents);
  waiter.Wait();
}

void StartOverview() {
  ash::Shell::Get()->overview_controller()->StartOverview();
}

void EndOverview() {
  ash::Shell::Get()->overview_controller()->EndOverview();
}

bool IsShelfVisible() {
  return ash::ShelfTestApi().IsVisible();
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
  TopChromeMdParamTest() : touch_ui_scoper_(GetParam()) {}
  ~TopChromeMdParamTest() override = default;

 private:
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_;
};

// Template to be used when a test does not work with the webUI tabstrip.
template <bool kEnabled, class BaseTest>
class WebUiTabStripOverrideTest : public BaseTest {
 public:
  WebUiTabStripOverrideTest() {
    if (kEnabled)
      feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
    else
      feature_override_.InitAndDisableFeature(features::kWebUITabStrip);
  }
  ~WebUiTabStripOverrideTest() override = default;

 private:
  base::test::ScopedFeatureList feature_override_;
};

// A helper class for immersive mode tests.
class ImmersiveModeTester : public ImmersiveModeController::Observer {
 public:
  explicit ImmersiveModeTester(Browser* browser) : browser_(browser) {
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
    ASSERT_FALSE(GetBrowserView()->immersive_mode_controller()->IsEnabled());
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
using BrowserNonClientFrameViewAshTestNoWebUiTabStrip =
    WebUiTabStripOverrideTest<false, BrowserNonClientFrameViewAshTest>;
using BrowserNonClientFrameViewAshTestWithWebUiTabStrip =
    WebUiTabStripOverrideTest<true, BrowserNonClientFrameViewAshTest>;

// This test does not make sense for the webUI tabstrip, since the window layout
// is different in that case.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTestNoWebUiTabStrip,
                       NonClientHitTest) {
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
  gfx::Point below_resize(kWindowWidth / 2, chromeos::kResizeInsideBoundsSize);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(below_resize));

  // Click in the top edge of a maximized window now hits the client area,
  // because we want it to fall through to the tab strip and select a tab.
  widget->Maximize();
  int expected_value = HTCLIENT;
  EXPECT_EQ(expected_value, frame_view->NonClientHitTest(top_edge));
}

// Test that the frame view does not do any painting in non-immersive
// fullscreen.
// This test does not make sense for the webUI tabstrip, since the frame is not
// painted in that case.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTestNoWebUiTabStrip,
                       NonImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());

  // No painting should occur in non-immersive fullscreen. (We enter into tab
  // fullscreen here because tab fullscreen is non-immersive even on ChromeOS).
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(frame_view->ShouldPaint());

  // The client view abuts top of the window.
  EXPECT_EQ(0, frame_view->GetBoundsForClientView().y());

  // The frame should be painted again when fullscreen is exited and the caption
  // buttons should be visible.
  ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(frame_view->ShouldPaint());
}

// Tests that Avatar icon should show on the top left corner of the teleported
// browser window on ChromeOS.
// TODO(http://crbug.com/1059514): This test should be made to work with the
// webUI tabstrip.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTestNoWebUiTabStrip,
                       AvatarDisplayOnTeleportedWindow) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);
  aura::Window* window = browser()->window()->GetNativeWindow();

  EXPECT_FALSE(MultiUserWindowManagerHelper::ShouldShowAvatar(window));
  EXPECT_FALSE(frame_view->profile_indicator_icon_);

  const AccountId account_id1 =
      multi_user_util::GetAccountIdFromProfile(browser()->profile());
  TestMultiUserWindowManager* window_manager =
      TestMultiUserWindowManager::Create(browser(), account_id1);

  // Teleport the window to another desktop.
  const AccountId account_id2(AccountId::FromUserEmail("user2"));
  window_manager->ShowWindowForUser(window, account_id2);
  EXPECT_TRUE(MultiUserWindowManagerHelper::ShouldShowAvatar(window));
  EXPECT_TRUE(frame_view->profile_indicator_icon_);

  // Teleport the window back to owner desktop.
  window_manager->ShowWindowForUser(window, account_id1);
  EXPECT_FALSE(MultiUserWindowManagerHelper::ShouldShowAvatar(window));
  EXPECT_FALSE(frame_view->profile_indicator_icon_);
}

// There should be no top inset when using the WebUI tab strip since the frame
// is invisible. Regression test for crbug.com/1076675
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTestWithWebUiTabStrip,
                       TopInset) {
  // This test doesn't make sense in non-touch mode since it expects the WebUI
  // tab strip to be active. This test is instantiated with and without touch
  // mode.
  if (!ui::TouchUiController::Get()->touch_ui())
    return;

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  StartOverview();
  EXPECT_EQ(0, GetFrameViewAsh(browser_view)->GetTopInset(false));

  EndOverview();
  EXPECT_EQ(0, GetFrameViewAsh(browser_view)->GetTopInset(false));
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       IncognitoMarkedAsAssistantBlocked) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(incognito_browser->window()->GetNativeWindow()->GetProperty(
      ash::kBlockedForAssistantSnapshotKey));
}

// Tests that browser frame minimum size constraint is updated in response to
// browser view layout.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       FrameMinSizeIsUpdated) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  BookmarkBarView* bookmark_bar = browser_view->GetBookmarkBarView();
  EXPECT_FALSE(bookmark_bar->GetVisible());
  const int min_height_no_bookmarks = frame_view->GetMinimumSize().height();

  // Setting non-zero bookmark bar preferred size forces it to be visible and
  // triggers BrowserView layout update.
  bookmark_bar->SetPreferredSize(gfx::Size(50, 5));
  browser_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(bookmark_bar->GetVisible());

  // Minimum window size should grow with the bookmark bar shown.
  gfx::Size min_window_size = frame_view->GetMinimumSize();
  EXPECT_GT(min_window_size.height(), min_height_no_bookmarks);
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       SettingsSystemWebAppHasMinimumWindowSize) {
  // Install the Settings System Web App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  // Open a settings window.
  auto* settings_manager = chrome::SettingsWindowManager::GetInstance();
  settings_manager->ShowOSSettings(browser()->profile());
  Browser* settings_browser =
      settings_manager->FindBrowserForProfile(browser()->profile());

  // Try to set the bounds to a tiny value.
  settings_browser->window()->SetBounds(gfx::Rect(1, 1));

  // The window has a reasonable size.
  gfx::Rect actual_bounds = settings_browser->window()->GetBounds();
  EXPECT_LE(300, actual_bounds.width());
  EXPECT_LE(100, actual_bounds.height());
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
  SessionRestoreTestHelper().Wait();

  Browser* new_browser = BrowserList::GetInstance()->GetLastActive();

  // Check that a layout occurs.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(new_browser);
  Widget* widget = browser_view->GetWidget();

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

  // TopChromeMdParamTest<InProcessBrowserTest>:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    BrowserView::SetDisableRevealerDelayForTesting(true);

    ash::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerAsh*>(
            BrowserView::GetBrowserViewForBrowser(browser())
                ->immersive_mode_controller())
            ->controller())
        .SetupForTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeBrowserViewTest);
};

}  // namespace

using ImmersiveModeBrowserViewTestNoWebUiTabStrip =
    WebUiTabStripOverrideTest<false, ImmersiveModeBrowserViewTest>;

// This test does not make sense for the webUI tabstrip, since the frame is not
// painted in that case.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTestNoWebUiTabStrip,
                       ImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();

  // Immersive fullscreen starts disabled.
  ASSERT_FALSE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());

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
  EXPECT_EQ(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());

  // Repeat test but without tab fullscreen.
  ExitFullscreenModeForTabAndWait(browser(), web_contents);

  // Immersive reveal should have same behavior as before.
  revealed_lock.reset(immersive_mode_controller->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());

  // Ending the reveal. Immersive browser should have the same behavior as full
  // screen, i.e., having an origin of (0,0).
  revealed_lock.reset();
  EXPECT_FALSE(frame_view->ShouldPaint());
  EXPECT_EQ(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());

  // Exiting immersive fullscreen should make the caption buttons and the frame
  // visible again.
  {
    FullscreenNotificationObserver waiter(browser());
    browser_view->ExitFullscreen();
    waiter.Wait();
  }
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());
}

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

// This test does not make sense for the webUI tabstrip, since the window layout
// is different in that case.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTestNoWebUiTabStrip,
                       TestCaptionButtonsReceiveEventsInBrowserImmersiveMode) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

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
  gfx::Size button_size = views::GetCaptionButtonLayoutSize(
      views::CaptionButtonLayoutSize::kBrowserCaptionMaximized);
  gfx::Point point_in_restore_button(window->GetBoundsInScreen().top_right());
  point_in_restore_button.Offset(-button_size.width() * 3 / 2,
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
  ASSERT_TRUE(browser->is_type_app());
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
  gfx::Size button_size = views::GetCaptionButtonLayoutSize(
      views::CaptionButtonLayoutSize::kBrowserCaptionMaximized);
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

// Regression test for crbug.com/796171.  Make sure that going from regular
// fullscreen to locked fullscreen does not cause a crash.
// Also test that the immersive mode is disabled afterwards (and the shelf is
// hidden, and the fullscreen control popup doesn't show up).
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       RegularToLockedFullscreenDisablesImmersive) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());

  // Set locked fullscreen state.
  browser()->window()->GetNativeWindow()->SetProperty(
      ash::kWindowPinTypeKey, ash::WindowPinType::kTrustedPinned);

  // We're fullscreen, immersive is disabled in locked fullscreen, and while
  // we're at it, also make sure that the shelf is hidden.
  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // Make sure the fullscreen control popup doesn't show up.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(),
                            base::TimeTicks(), 0, 0);
  ASSERT_TRUE(browser_view->fullscreen_control_host_for_test());
  browser_view->fullscreen_control_host_for_test()->OnMouseEvent(mouse_move);
  EXPECT_FALSE(browser_view->fullscreen_control_host_for_test()->IsVisible());
}

// Regression test for crbug.com/883104.  Make sure that immersive fullscreen is
// disabled in locked fullscreen mode (also the shelf is hidden, and the
// fullscreen control popup doesn't show up).
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       LockedFullscreenDisablesImmersive) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());

  // Set locked fullscreen state.
  browser()->window()->GetNativeWindow()->SetProperty(
      ash::kWindowPinTypeKey, ash::WindowPinType::kTrustedPinned);

  // We're fullscreen, immersive is disabled in locked fullscreen, and while
  // we're at it, also make sure that the shelf is hidden.
  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // Make sure the fullscreen control popup doesn't show up.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(),
                            base::TimeTicks(), 0, 0);
  ASSERT_TRUE(browser_view->fullscreen_control_host_for_test());
  browser_view->fullscreen_control_host_for_test()->OnMouseEvent(mouse_move);
  EXPECT_FALSE(browser_view->fullscreen_control_host_for_test()->IsVisible());
}

// Test the shelf visibility affected by entering and exiting tab fullscreen and
// immersive fullscreen.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest, TabAndBrowserFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);

  // The shelf should start out as visible.
  EXPECT_TRUE(IsShelfVisible());

  // 1) Test that entering tab fullscreen from immersive fullscreen hides the
  // shelf.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // 2) Test that exiting tab fullscreen autohides the shelf.
  ExitFullscreenModeForTabAndWait(browser(), web_contents);
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // 3) Test that exiting tab fullscreen and immersive fullscreen correctly
  // updates the shelf visibility.
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_TRUE(IsShelfVisible());
}

namespace {

class WebAppNonClientFrameViewAshTest
    : public TopChromeMdParamTest<BrowserActionsBarBrowserTest> {
 public:
  WebAppNonClientFrameViewAshTest() = default;

  ~WebAppNonClientFrameViewAshTest() override = default;

  GURL GetAppURL() const {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }

  static SkColor GetThemeColor() { return SK_ColorBLUE; }

  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  ash::DefaultFrameHeader* frame_header_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;
  const std::vector<ContentSettingImageView*>* content_setting_views_ = nullptr;
  BrowserActionsContainer* browser_actions_container_ = nullptr;
  AppMenuButton* web_app_menu_button_ = nullptr;

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

    WebAppFrameToolbarView::DisableAnimationForTesting();

    // Start secure local server.
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // |SetUpWebApp()| must be called after |SetUpOnMainThread()| to make sure
  // the Network Service process has been setup properly.
  void SetUpWebApp() {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->theme_color = GetThemeColor();

    web_app::AppId app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view_);
    frame_header_ =
        static_cast<ash::DefaultFrameHeader*>(frame_view->frame_header_.get());

    web_app_frame_toolbar_ = frame_view->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());

    content_setting_views_ =
        &web_app_frame_toolbar_->GetContentSettingViewsForTesting();
    browser_actions_container_ =
        web_app_frame_toolbar_->GetBrowserActionsContainer();
    web_app_menu_button_ = web_app_frame_toolbar_->GetAppMenuButton();
  }

  AppMenu* GetAppMenu() { return web_app_menu_button_->app_menu(); }

  SkColor GetActiveColor() const {
    return web_app_frame_toolbar_->active_foreground_color_;
  }

  bool GetPaintingAsActive() const {
    return web_app_frame_toolbar_->paint_as_active_;
  }

  PageActionIconView* GetPageActionIcon(PageActionIconType type) {
    return browser_view_->toolbar_button_provider()->GetPageActionIconView(
        type);
  }

  ContentSettingImageView* GrantGeolocationPermission() {
    content::RenderFrameHost* frame =
        app_browser_->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    content_settings::PageSpecificContentSettings* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            frame->GetProcess()->GetID(), frame->GetRoutingID());
    content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);

    return *std::find_if(
        content_setting_views_->begin(), content_setting_views_->end(),
        [](const auto* view) {
          return view->GetTypeForTesting() ==
                 ContentSettingImageModel::ImageType::GEOLOCATION;
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
  // For mocking a secure site.
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(WebAppNonClientFrameViewAshTest);
};

}  // namespace

// Tests that the page info dialog doesn't anchor in a way that puts it outside
// of web-app windows. This is important as some platforms don't support bubble
// anchor adjustment (see |BubbleDialogDelegateView::CreateBubble()|).
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       PageInfoBubblePosition) {
  SetUpWebApp();
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
  chrome::ExecuteCommand(app_browser_, IDC_WEB_APP_MENU_APP_INFO);

  // Check the bubble anchors inside the main app window even if there's space
  // available outside the main app window.
  gfx::Rect page_info_bounds =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()
          ->GetWidget()
          ->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget->GetWindowBoundsInScreen().Contains(page_info_bounds));
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, FocusableViews) {
  SetUpWebApp();
  EXPECT_TRUE(browser_view_->contents_web_view()->HasFocus());
  browser_view_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(web_app_menu_button_->HasFocus());
  browser_view_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_TRUE(browser_view_->contents_web_view()->HasFocus());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       ButtonVisibilityInOverviewMode) {
  SetUpWebApp();
  EXPECT_TRUE(web_app_frame_toolbar_->GetVisible());

  StartOverview();
  EXPECT_FALSE(web_app_frame_toolbar_->GetVisible());
  EndOverview();
  EXPECT_TRUE(web_app_frame_toolbar_->GetVisible());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, FrameThemeColorIsSet) {
  SetUpWebApp();
  aura::Window* window = browser_view_->GetWidget()->GetNativeWindow();
  EXPECT_EQ(GetThemeColor(), window->GetProperty(ash::kFrameActiveColorKey));
  EXPECT_EQ(GetThemeColor(), window->GetProperty(ash::kFrameInactiveColorKey));
  EXPECT_EQ(gfx::kGoogleGrey200, GetActiveColor());
}

// Make sure that for web apps, the height of the frame doesn't exceed the
// height of the caption buttons.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, FrameSize) {
  SetUpWebApp();
  const int inset = GetFrameViewAsh(browser_view_)->GetTopInset(false);
  EXPECT_EQ(inset, views::GetCaptionButtonLayoutSize(
                       views::CaptionButtonLayoutSize::kNonBrowserCaption)
                       .height());
  EXPECT_GE(inset, web_app_menu_button_->size().height());
  EXPECT_GE(inset, web_app_frame_toolbar_->size().height());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       IsToolbarButtonProvider) {
  SetUpWebApp();
  EXPECT_EQ(browser_view_->toolbar_button_provider(), web_app_frame_toolbar_);
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       ShowManagePasswordsIcon) {
  SetUpWebApp();
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  PageActionIconView* manage_passwords_icon =
      GetPageActionIcon(PageActionIconType::kManagePasswords);

  EXPECT_TRUE(manage_passwords_icon);
  EXPECT_FALSE(manage_passwords_icon->GetVisible());

  autofill::PasswordForm password_form;
  password_form.username_value = base::ASCIIToUTF16("test");
  password_form.url = GetAppURL().GetOrigin();
  PasswordsClientUIDelegateFromWebContents(web_contents)
      ->OnPasswordAutofilled({&password_form},
                             url::Origin::Create(password_form.url), nullptr);
  chrome::ManagePasswordsForPage(app_browser_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(manage_passwords_icon->GetVisible());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, ShowZoomIcon) {
  SetUpWebApp();
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  PageActionIconView* zoom_icon = GetPageActionIcon(PageActionIconType::kZoom);

  EXPECT_TRUE(zoom_icon);
  EXPECT_FALSE(zoom_icon->GetVisible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  zoom_controller->SetZoomLevel(blink::PageZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(zoom_icon->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, ShowFindIcon) {
  SetUpWebApp();
  PageActionIconView* find_icon = GetPageActionIcon(PageActionIconType::kFind);

  EXPECT_TRUE(find_icon);
  EXPECT_FALSE(find_icon->GetVisible());

  chrome::Find(app_browser_);

  EXPECT_TRUE(find_icon->GetVisible());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, ShowTranslateIcon) {
  SetUpWebApp();
  PageActionIconView* translate_icon =
      GetPageActionIcon(PageActionIconType::kTranslate);

  ASSERT_TRUE(translate_icon);
  EXPECT_FALSE(translate_icon->GetVisible());

  chrome::Find(app_browser_);
  browser_view_->ShowTranslateBubble(browser_view_->GetActiveWebContents(),
                                     translate::TRANSLATE_STEP_AFTER_TRANSLATE,
                                     "en", "fr",
                                     translate::TranslateErrors::NONE, true);

  EXPECT_TRUE(translate_icon->GetVisible());
}

// Tests that the focus toolbar command focuses the app menu button in web-app
// windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       BrowserCommandFocusToolbarAppMenu) {
  SetUpWebApp();
  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_TOOLBAR);
  EXPECT_TRUE(web_app_menu_button_->HasFocus());
}

// TODO(): Flaky crash on Chrome OS debug.
#if defined(OS_CHROMEOS)
#define MAYBE_BrowserCommandFocusToolbarGeolocation \
  DISABLED_BrowserCommandFocusToolbarGeolocation
#else
#define MAYBE_BrowserCommandFocusToolbarGeolocation \
  BrowserCommandFocusToolbarGeolocation
#endif
// Tests that the focus toolbar command focuses content settings icons before
// the app menu button when present in web-app windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       MAYBE_BrowserCommandFocusToolbarGeolocation) {
  SetUpWebApp();
  ContentSettingImageView* geolocation_icon = GrantGeolocationPermission();

  // In order to receive focus, the geo icon must be laid out (and be both
  // visible and nonzero size).
  web_app_frame_toolbar_->Layout();

  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  EXPECT_FALSE(geolocation_icon->HasFocus());

  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_TOOLBAR);

  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  EXPECT_TRUE(geolocation_icon->HasFocus());
}

// Tests that the show app menu command opens the app menu for web-app windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       BrowserCommandShowAppMenu) {
  SetUpWebApp();
  EXPECT_EQ(nullptr, GetAppMenu());
  chrome::ExecuteCommand(app_browser_, IDC_SHOW_APP_MENU);
  EXPECT_NE(nullptr, GetAppMenu());
}

// Tests that the focus next pane command focuses the app menu for web-app
// windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       BrowserCommandFocusNextPane) {
  SetUpWebApp();
  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_NEXT_PANE);
  EXPECT_TRUE(web_app_menu_button_->HasFocus());
}

// Tests that the custom tab bar is focusable from the keyboard.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       CustomTabBarIsFocusable) {
  SetUpWebApp();

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(app_browser_);

  const GURL kOutOfScopeURL("http://example.org/");
  NavigateParams nav_params(app_browser_, kOutOfScopeURL,
                            ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&nav_params);
  auto* custom_tab_bar = browser_view->toolbar()->custom_tab_bar();

  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_NEXT_PANE);
  ASSERT_TRUE(web_app_menu_button_->HasFocus());

  EXPECT_FALSE(custom_tab_bar->close_button_for_testing()->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_NEXT_PANE);
  EXPECT_TRUE(custom_tab_bar->close_button_for_testing()->HasFocus());
}

// Tests that the focus previous pane command focuses the app menu for web-app
// windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       BrowserCommandFocusPreviousPane) {
  SetUpWebApp();
  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_PREVIOUS_PANE);
  EXPECT_TRUE(web_app_menu_button_->HasFocus());
}

// Tests that a web app's content settings icons can be interacted with.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, ContentSettingIcons) {
  SetUpWebApp();
  for (auto* view : *content_setting_views_)
    EXPECT_FALSE(view->GetVisible());

  ContentSettingImageView* geolocation_icon = GrantGeolocationPermission();

  for (auto* view : *content_setting_views_) {
    bool is_geolocation_icon = view == geolocation_icon;
    EXPECT_EQ(is_geolocation_icon, view->GetVisible());
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
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, BrowserActions) {
  SetUpWebApp();
  // Even though 2 are visible in the browser, no extension actions should show.
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions_container_->toolbar_actions_bar();
  LoadExtensions();
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(0u, browser_actions_container_->VisibleBrowserActions());

  // Show the menu.
  SimulateClickOnView(web_app_menu_button_);

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
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest,
                       ActiveStateOfButtonMatchesWidget) {
  SetUpWebApp();
  ash::FrameCaptionButtonContainerView::TestApi test(
      GetFrameViewAsh(browser_view_)->caption_button_container_);
  EXPECT_TRUE(test.size_button()->paint_as_active());
  EXPECT_TRUE(GetPaintingAsActive());

  browser_view_->GetWidget()->Deactivate();
  EXPECT_FALSE(test.size_button()->paint_as_active());
  EXPECT_FALSE(GetPaintingAsActive());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewAshTest, PopupHasNoToolbar) {
  SetUpWebApp();
  {
    NavigateParams navigate_params(app_browser_, GetAppURL(),
                                   ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition = WindowOpenDisposition::NEW_POPUP;

    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    Navigate(&navigate_params);
    navigation_observer.WaitForNavigationFinished();
  }

  Browser* popup_browser = BrowserList::GetInstance()->GetLastActive();
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(popup_browser);
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);
  EXPECT_FALSE(frame_view->web_app_frame_toolbar_for_testing());
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

// Test that for a browser window, its caption buttons are always hidden in
// tablet mode.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       BrowserHeaderVisibilityInTabletModeTest) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize |
          aura::client::kResizeBehaviorCanResize);
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());

  StartOverview();
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());
  EndOverview();
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());

  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(true));
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());
  StartOverview();
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());
  EndOverview();
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());
  ash::SplitViewTestApi().SnapWindow(widget->GetNativeWindow(),
                                     ash::SplitViewTestApi::SnapPosition::LEFT);
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());
}

// Test that for a browser app window, its caption buttons may or may not hide
// in tablet mode.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       AppHeaderVisibilityInTabletModeTest) {
  // Create a browser app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser2 = new Browser(params);
  AddBlankTabAndShow(browser2);
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  Widget* widget2 = browser_view2->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view2 = GetFrameViewAsh(browser_view2);
  widget2->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize |
          aura::client::kResizeBehaviorCanResize);
  StartOverview();
  EXPECT_FALSE(frame_view2->caption_button_container_->GetVisible());
  EndOverview();
  EXPECT_TRUE(frame_view2->caption_button_container_->GetVisible());

  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(true));
  StartOverview();
  EXPECT_FALSE(frame_view2->caption_button_container_->GetVisible());

  EndOverview();
  EXPECT_TRUE(frame_view2->caption_button_container_->GetVisible());

  ash::SplitViewTestApi().SnapWindow(
      widget2->GetNativeWindow(), ash::SplitViewTestApi::SnapPosition::RIGHT);
  EXPECT_TRUE(frame_view2->caption_button_container_->GetVisible());
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
  StartOverview();
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

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeLauncherBrowserNonClientFrameViewAshTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(HomeLauncherBrowserNonClientFrameViewAshTest,
                       TabletModeBrowserCaptionButtonVisibility) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);

  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());
  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(true));
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());

  StartOverview();
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());
  EndOverview();
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());

  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(false));
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());
}

// TODO(crbug.com/993974): When the test flake has been addressed, improve
// performance by consolidating this unit test with
// |TabletModeBrowserCaptionButtonVisibility|. Do not forget to remove the
// corresponding |FRIEND_TEST_ALL_PREFIXES| usage from
// |BrowserNonClientFrameViewAsh|.
IN_PROC_BROWSER_TEST_P(HomeLauncherBrowserNonClientFrameViewAshTest,
                       CaptionButtonVisibilityForBrowserLaunchedInTabletMode) {
  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(true));
  EXPECT_FALSE(GetFrameViewAsh(BrowserView::GetBrowserViewForBrowser(
                                   CreateBrowser(browser()->profile())))
                   ->caption_button_container_->GetVisible());
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
  ASSERT_TRUE(browser->is_type_app());
  browser->window()->Show();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  BrowserNonClientFrameViewAsh* frame_view = GetFrameViewAsh(browser_view);
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());

  // Tablet mode doesn't affect app's caption button's visibility.
  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(true));
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());

  // However, overview mode does.
  StartOverview();
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());
  EndOverview();
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());

  ASSERT_NO_FATAL_FAILURE(
      ash::ShellTestApi().SetTabletModeEnabledForTest(false));
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());
}

#define INSTANTIATE_TEST_SUITE(name) \
  INSTANTIATE_TEST_SUITE_P(All, name, ::testing::Values(false, true))

INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewAshTest);
INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewAshTestNoWebUiTabStrip);
INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewAshTestWithWebUiTabStrip);
INSTANTIATE_TEST_SUITE(ImmersiveModeBrowserViewTest);
INSTANTIATE_TEST_SUITE(ImmersiveModeBrowserViewTestNoWebUiTabStrip);
INSTANTIATE_TEST_SUITE(WebAppNonClientFrameViewAshTest);
INSTANTIATE_TEST_SUITE(HomeLauncherBrowserNonClientFrameViewAshTest);
