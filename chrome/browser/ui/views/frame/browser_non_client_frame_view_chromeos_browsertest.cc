// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"

#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chromeos/test_util.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "components/account_id/account_id.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/multi_user/test_multi_user_window_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "content/public/test/background_color_change_waiter.h"
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/lacros/window_properties.h"
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#endif

namespace {

bool WaitForFocus(bool expected, views::View* view) {
  return base::test::RunUntil([&]() { return view->HasFocus() == expected; });
}

bool WaitForVisible(bool expected, views::View* view) {
  return base::test::RunUntil([&]() { return view->GetVisible() == expected; });
}

bool WaitForPaintAsActive(bool expected, views::FrameCaptionButton* button) {
  return base::test::RunUntil(
      [&]() { return button->GetPaintAsActive() == expected; });
}

}  // namespace

class BrowserNonClientFrameViewChromeOSTest
    : public TopChromeMdParamTest<ChromeOSBrowserUITest> {
 protected:
  BrowserNonClientFrameViewChromeOSTest()
      : scoped_features_(
            chromeos::features::kOverviewSessionInitOptimizations) {}

  base::test::ScopedFeatureList scoped_features_;
};

using BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip =
    WebUiTabStripOverrideTest<false, BrowserNonClientFrameViewChromeOSTest>;
using BrowserNonClientFrameViewChromeOSTestWithWebUiTabStrip =
    WebUiTabStripOverrideTest<true, BrowserNonClientFrameViewChromeOSTest>;

class BrowserNonClientFrameViewChromeOSTestApi {
 public:
  explicit BrowserNonClientFrameViewChromeOSTestApi(
      BrowserNonClientFrameViewChromeOS* frame_view)
      : frame_view_(frame_view) {}
  BrowserNonClientFrameViewChromeOSTestApi(
      const BrowserNonClientFrameViewChromeOSTestApi&) = delete;
  BrowserNonClientFrameViewChromeOSTestApi& operator=(
      const BrowserNonClientFrameViewChromeOSTestApi&) = delete;
  ~BrowserNonClientFrameViewChromeOSTestApi() = default;

  bool GetShouldPaint() const { return frame_view_->GetShouldPaint(); }

  ProfileIndicatorIcon* GetProfileIndicatorIcon() {
    return frame_view_->profile_indicator_icon_;
  }

  chromeos::FrameHeader* GetFrameHeader() {
    return frame_view_->frame_header_.get();
  }

 private:
  const raw_ptr<BrowserNonClientFrameViewChromeOS> frame_view_;
};

// This test does not make sense for the webUI tabstrip, since the window layout
// is different in that case.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip,
                       NonClientHitTest) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

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
  {
    gfx::Rect old_bounds = frame_view->bounds();
    widget->Maximize();
    auto* window = widget->GetNativeWindow();
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return window->GetProperty(chromeos::kWindowStateTypeKey) ==
             chromeos::WindowStateType::kMaximized;
    }));
    // TODO(crbug.com/40276379): Remove waiting for bounds change when the bug
    // is fixed.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return frame_view->bounds() != old_bounds; }));
  }
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(top_edge));
}

// Regression test for crbug.com/40945061. Asserts that the content window
// accepts input from the edge of the browser frame when the browser is
// maximized.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip,
                       ContentWindowAcceptsEdgeInputsWhenMaximized) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  views::Widget* widget = browser_view->GetWidget();
  const BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

  // Maximize the widget.
  EXPECT_FALSE(widget->IsMaximized());
  const gfx::Rect old_bounds = frame_view->bounds();
  widget->Maximize();
  auto* window = widget->GetNativeWindow();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return window->GetProperty(chromeos::kWindowStateTypeKey) ==
           chromeos::WindowStateType::kMaximized;
  }));
  // TODO(crbug.com/40276379): Remove waiting for bounds change when the bug
  // is fixed.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return frame_view->bounds() != old_bounds; }));

  // Assert that input events at the edge of the browser are propagated to the
  // web contents window.
  EXPECT_FALSE(web_contents->GetFocusedFrame());
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  ASSERT_NO_FATAL_FAILURE(
      event_generator.GestureTapAt(frame_view->bounds().left_center()));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !!web_contents->GetFocusedFrame(); }));
}

using BrowserNonClientFrameViewChromeOSTouchTest =
    TopChromeTouchTest<ChromeOSBrowserUITest>;

using BrowserNonClientFrameViewChromeOSTouchTestWithWebUiTabStrip =
    WebUiTabStripOverrideTest<true, BrowserNonClientFrameViewChromeOSTouchTest>;

IN_PROC_BROWSER_TEST_F(
    BrowserNonClientFrameViewChromeOSTouchTestWithWebUiTabStrip,
    TabletSplitViewNonClientHitTest) {
  if (!IsSnapWindowSupported()) {
    GTEST_SKIP() << "Ash is too old.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);
  views::Widget* widget = browser_view->GetWidget();
  aura::Window* window = widget->GetNativeWindow();

  const int expect_y =
      frame_view->GetBorder() ? frame_view->GetBorder()->GetInsets().top() : 0;
  EXPECT_EQ(expect_y, frame_view->GetBoundsForClientView().y());

  EnterTabletMode();
  SnapWindow(window, crosapi::mojom::SnapPosition::kPrimary);

  // Touch on the top of the window is interpreted as client hit.
  gfx::Point top_point(widget->GetWindowBoundsInScreen().width() / 2, 0);
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(top_point));
}

IN_PROC_BROWSER_TEST_F(
    BrowserNonClientFrameViewChromeOSTouchTestWithWebUiTabStrip,
    TabletSplitViewSwipeDownFromEdgeOpensWebUiTabStrip) {
  if (!IsSnapWindowSupported()) {
    GTEST_SKIP() << "Ash is too old.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);
  const int expect_y =
      frame_view->GetBorder() ? frame_view->GetBorder()->GetInsets().top() : 0;
  EXPECT_EQ(expect_y, frame_view->GetBoundsForClientView().y());
  views::Widget* widget = browser_view->GetWidget();

  EnterTabletMode();
  SnapWindow(widget->GetNativeWindow(), crosapi::mojom::SnapPosition::kPrimary);

  // A point at the top of the window, but not in the center horizontally, as a
  // swipe down from the top center will show the chromeos tablet mode multitask
  // menu.
  gfx::Point edge_point(100, 0);

  ASSERT_FALSE(browser_view->webui_tab_strip()->GetVisible());
  aura::Window* window = widget->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  event_generator.SetTouchRadius(10, 5);
  event_generator.PressTouch(edge_point);
  event_generator.MoveTouchBy(0, 100);
  event_generator.ReleaseTouch();
  ASSERT_TRUE(WaitForVisible(true, browser_view->webui_tab_strip()));
}

// Test that the frame view does not do any painting in non-immersive
// fullscreen.
// This test does not make sense for the webUI tabstrip, since the frame is not
// painted in that case.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40276379): Reenable when bug is fixed.
#define MAYBE_NonImmersiveFullscreen DISABLED_NonImmersiveFullscreen
#else
#define MAYBE_NonImmersiveFullscreen NonImmersiveFullscreen
#endif
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip,
                       MAYBE_NonImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);
  BrowserNonClientFrameViewChromeOSTestApi test_api(frame_view);

  // Frame paints by default.
  EXPECT_TRUE(test_api.GetShouldPaint());

  // No painting should occur in non-immersive fullscreen. (We enter into tab
  // fullscreen here because tab fullscreen is non-immersive even on ChromeOS).
  EnterTabFullscreenMode(browser(), web_contents);
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_TRUE(browser_view->IsFullscreen());
  EXPECT_FALSE(test_api.GetShouldPaint());

  // The client view abuts top of the window.
  EXPECT_EQ(0, frame_view->GetBoundsForClientView().y());

  // The frame should be painted again when fullscreen is exited and the caption
  // buttons should be visible.
  ExitTabFullscreenMode(browser(), web_contents);
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(test_api.GetShouldPaint());
}

// Tests that caption buttons are hidden when entering tab fullscreen.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40276379): Reenable when bug is fixed.
#define MAYBE_CaptionButtonsHiddenNonImmersiveFullscreen \
  DISABLED_CaptionButtonsHiddenNonImmersiveFullscreen
#else
#define MAYBE_CaptionButtonsHiddenNonImmersiveFullscreen \
  CaptionButtonsHiddenNonImmersiveFullscreen
#endif
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip,
                       MAYBE_CaptionButtonsHiddenNonImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

  EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());

  EnterTabFullscreenMode(browser(), web_contents);
  EXPECT_TRUE(browser_view->IsFullscreen());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  // Caption buttons are hidden.
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());

  // The frame should be painted again when fullscreen is exited and the caption
  // buttons should be visible.
  ExitTabFullscreenMode(browser(), web_contents);
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser_view->IsFullscreen());
  // Caption button container visible again.
  EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
}

// There should be no top inset when using the WebUI tab strip since the frame
// is invisible. Regression test for crbug.com/1076675
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTestWithWebUiTabStrip,
                       TopInset) {
  // This test doesn't make sense in non-touch mode since it expects the WebUI
  // tab strip to be active. This test is instantiated with and without touch
  // mode.
  if (!ui::TouchUiController::Get()->touch_ui()) {
    return;
  }

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  EXPECT_EQ(0, GetFrameViewChromeOS(browser_view)->GetTopInset(false));
  EnterOverviewMode();
  EXPECT_EQ(0, GetFrameViewChromeOS(browser_view)->GetTopInset(false));
  ExitOverviewMode();
  EXPECT_EQ(0, GetFrameViewChromeOS(browser_view)->GetTopInset(false));
}

// Tests to ensure caption buttons are not painted when the WebUI tab strip is
// present for the browser window (crbug.com/1362731).
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTestWithWebUiTabStrip,
                       CaptionButtonsHiddenWhenUsingWebUITabStrip) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* frame_view = GetFrameViewChromeOS(browser_view);
  if (ui::TouchUiController::Get()->touch_ui()) {
    EXPECT_TRUE(browser_view->webui_tab_strip());
    EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  } else {
    EXPECT_FALSE(browser_view->webui_tab_strip());
    EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       IncognitoMarkedAsAssistantBlocked) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(incognito_browser->window()->GetNativeWindow()->GetProperty(
      chromeos::kBlockedForAssistantSnapshotKey));
}

// Tests that browser frame minimum size constraint is updated in response to
// browser view layout.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       FrameMinSizeIsUpdated) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

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

// This is a regression test that session restore minimized browser should
// re-layout the header (https://crbug.com/827444).
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       RestoreMinimizedBrowserUpdatesCaption) {
  // Enable session service.
  SessionStartupPref pref(SessionStartupPref::LAST);
  Profile* profile = browser()->profile();
  SessionStartupPref::SetStartupPref(profile, pref);

  SessionServiceTestHelper helper(profile);
  helper.SetForceBrowserNotAliveWithNoWindows(true);

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
  views::Widget* widget = browser_view->GetWidget();

  BrowserNonClientFrameViewChromeOS* frame_view =
      static_cast<BrowserNonClientFrameViewChromeOS*>(
          widget->non_client_view()->frame_view());

  chromeos::FrameCaptionButtonContainerView::TestApi test(
      frame_view->caption_button_container());
  EXPECT_TRUE(test.size_button()->icon_definition_for_test());
}

namespace {

class WebAppNonClientFrameViewChromeOSTest
    : public TopChromeMdParamTest<ChromeOSBrowserUITest> {
 public:
  WebAppNonClientFrameViewChromeOSTest() = default;
  WebAppNonClientFrameViewChromeOSTest(
      const WebAppNonClientFrameViewChromeOSTest&) = delete;
  WebAppNonClientFrameViewChromeOSTest& operator=(
      const WebAppNonClientFrameViewChromeOSTest&) = delete;
  ~WebAppNonClientFrameViewChromeOSTest() override = default;

  GURL GetAppURL() const {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }

  static SkColor GetThemeColor() { return SK_ColorBLUE; }

  raw_ptr<Browser, DanglingUntriaged> app_browser_ = nullptr;
  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<chromeos::DefaultFrameHeader, DanglingUntriaged> frame_header_ =
      nullptr;
  raw_ptr<WebAppFrameToolbarView, DanglingUntriaged> web_app_frame_toolbar_ =
      nullptr;
  raw_ptr<
      const std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>,
      DanglingUntriaged>
      content_setting_views_ = nullptr;
  raw_ptr<AppMenuButton, DanglingUntriaged> web_app_menu_button_ = nullptr;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TopChromeMdParamTest<ChromeOSBrowserUITest>::SetUpCommandLine(command_line);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    TopChromeMdParamTest<
        ChromeOSBrowserUITest>::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
    TopChromeMdParamTest<
        ChromeOSBrowserUITest>::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    TopChromeMdParamTest<ChromeOSBrowserUITest>::SetUpOnMainThread();

    WebAppToolbarButtonContainer::DisableAnimationForTesting(true);

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
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(GetAppURL());
    web_app_info->scope = GetAppURL().GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->theme_color = GetThemeColor();

    webapps::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    ASSERT_TRUE(browser_test_util::WaitForWindowCreation(app_browser_));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    BrowserNonClientFrameViewChromeOS* frame_view =
        GetFrameViewChromeOS(browser_view_);
    frame_header_ = static_cast<chromeos::DefaultFrameHeader*>(
        BrowserNonClientFrameViewChromeOSTestApi(frame_view).GetFrameHeader());

    web_app_frame_toolbar_ = browser_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());

    content_setting_views_ =
        &web_app_frame_toolbar_->GetContentSettingViewsForTesting();
    web_app_menu_button_ = web_app_frame_toolbar_->GetAppMenuButton();
  }

  AppMenu* GetAppMenu() { return web_app_menu_button_->app_menu(); }

  SkColor GetActiveColor() const {
    return *web_app_frame_toolbar_->active_foreground_color_;
  }

  bool GetPaintingAsActive() const {
    return web_app_frame_toolbar_->paint_as_active_;
  }

  PageActionIconView* GetPageActionIcon(PageActionIconType type) {
    return browser_view_->toolbar_button_provider()->GetPageActionIconView(
        type);
  }

  ContentSettingImageView* GrantGeolocationPermission() {
    content::RenderFrameHost* frame = app_browser_->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame();
    content_settings::PageSpecificContentSettings* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(frame);
    content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);

    return *base::ranges::find(*content_setting_views_,
                               ContentSettingImageModel::ImageType::GEOLOCATION,
                               &ContentSettingImageView::GetType);
  }

  void SimulateClickOnView(views::View* view) {
    const gfx::Point point;
    ui::MouseEvent event(ui::EventType::kMousePressed, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseEvent(&event);
    ui::MouseEvent event_rel(ui::EventType::kMouseReleased, point, point,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseEvent(&event_rel);
  }

 private:
  // For mocking a secure site.
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier cert_verifier_;
};

}  // namespace

// Tests that the page info dialog doesn't anchor in a way that puts it outside
// of web-app windows. This is important as some platforms don't support bubble
// anchor adjustment (see |BubbleDialogDelegateView::CreateBubble()|).
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
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

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest, FocusableViews) {
  SetUpWebApp();
  ASSERT_TRUE(WaitForFocus(true, browser_view_->contents_web_view()));
  browser_view_->GetFocusManager()->AdvanceFocus(false);
  ASSERT_TRUE(WaitForFocus(true, web_app_menu_button_));
  browser_view_->GetFocusManager()->AdvanceFocus(false);
  ASSERT_TRUE(WaitForFocus(true, browser_view_->contents_web_view()));
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       ButtonVisibilityInOverviewMode) {
  SetUpWebApp();
  ASSERT_TRUE(WaitForVisible(true, web_app_frame_toolbar_));

  EnterOverviewMode();
  views::test::RunScheduledLayout(browser_view_);
  ASSERT_TRUE(WaitForVisible(false, web_app_frame_toolbar_));

  ExitOverviewMode();
  views::test::RunScheduledLayout(browser_view_);
  ASSERT_TRUE(WaitForVisible(true, web_app_frame_toolbar_));
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       FrameThemeColorIsSet) {
  SetUpWebApp();
  aura::Window* window = browser_view_->GetWidget()->GetNativeWindow();
  EXPECT_EQ(GetThemeColor(),
            window->GetProperty(chromeos::kFrameActiveColorKey));
  EXPECT_EQ(GetThemeColor(),
            window->GetProperty(chromeos::kFrameInactiveColorKey));
  EXPECT_EQ(gfx::kGoogleGrey200, GetActiveColor());
}

// Make sure that for web apps, the height of the frame doesn't exceed the
// height of the caption buttons.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest, FrameSize) {
  SetUpWebApp();
  const int inset = GetFrameViewChromeOS(browser_view_)->GetTopInset(false);
  EXPECT_EQ(inset, views::GetCaptionButtonLayoutSize(
                       views::CaptionButtonLayoutSize::kNonBrowserCaption)
                       .height());
  EXPECT_GE(inset, web_app_menu_button_->size().height());
  EXPECT_GE(inset, web_app_frame_toolbar_->size().height());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       IsToolbarButtonProvider) {
  SetUpWebApp();
  EXPECT_EQ(browser_view_->toolbar_button_provider(), web_app_frame_toolbar_);
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       ShowManagePasswordsIcon) {
  SetUpWebApp();
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  PageActionIconView* manage_passwords_icon =
      GetPageActionIcon(PageActionIconType::kManagePasswords);

  EXPECT_TRUE(manage_passwords_icon);
  EXPECT_FALSE(manage_passwords_icon->GetVisible());

  password_manager::PasswordForm password_form;
  password_form.username_value = u"test";
  password_form.url = GetAppURL().DeprecatedGetOriginAsURL();
  password_form.match_type = password_manager::PasswordForm::MatchType::kExact;
  std::vector<password_manager::PasswordForm> forms = {password_form};
  PasswordsClientUIDelegateFromWebContents(web_contents)
      ->OnPasswordAutofilled(forms, url::Origin::Create(password_form.url), {});
  chrome::ManagePasswordsForPage(app_browser_);
  ASSERT_TRUE(WaitForVisible(true, manage_passwords_icon));
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest, ShowZoomIcon) {
  SetUpWebApp();
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  PageActionIconView* zoom_icon = GetPageActionIcon(PageActionIconType::kZoom);

  EXPECT_TRUE(zoom_icon);
  EXPECT_FALSE(zoom_icon->GetVisible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.5));
  ASSERT_TRUE(WaitForVisible(true, zoom_icon));
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest, ShowFindIcon) {
  SetUpWebApp();
  PageActionIconView* find_icon = GetPageActionIcon(PageActionIconType::kFind);

  EXPECT_TRUE(find_icon);
  EXPECT_FALSE(find_icon->GetVisible());

  chrome::Find(app_browser_);

  ASSERT_TRUE(WaitForVisible(true, find_icon));
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       ShowTranslateIcon) {
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

  ASSERT_TRUE(WaitForVisible(true, translate_icon));
}

// Tests that the focus toolbar command focuses the app menu button in web-app
// windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       BrowserCommandFocusToolbarAppMenu) {
  SetUpWebApp();
  ASSERT_TRUE(WaitForFocus(true, browser_view_->contents_web_view()));

  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_TOOLBAR);
  ASSERT_TRUE(WaitForFocus(true, web_app_menu_button_));
}

// Tests that the focus toolbar command focuses content settings icons before
// the app menu button when present in web-app windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       BrowserCommandFocusToolbarGeolocation) {
  SetUpWebApp();

  ContentSettingImageView* geolocation_icon = GrantGeolocationPermission();

  // In order to receive focus, the geo icon must be laid out (and be both
  // visible and nonzero size).
  RunScheduledLayouts();

  ASSERT_TRUE(WaitForFocus(true, browser_view_->contents_web_view()));
  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  EXPECT_FALSE(geolocation_icon->HasFocus());

  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_TOOLBAR);

  ASSERT_TRUE(WaitForFocus(true, geolocation_icon));
  EXPECT_FALSE(web_app_menu_button_->HasFocus());
}

// Tests that the show app menu command opens the app menu for web-app windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       BrowserCommandShowAppMenu) {
  SetUpWebApp();
  EXPECT_EQ(nullptr, GetAppMenu());
  chrome::ExecuteCommand(app_browser_, IDC_SHOW_APP_MENU);
  EXPECT_NE(nullptr, GetAppMenu());
}

// Tests that the focus next pane command focuses the app menu for web-app
// windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       BrowserCommandFocusNextPane) {
  SetUpWebApp();
  ASSERT_TRUE(WaitForFocus(true, browser_view_->contents_web_view()));
  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_NEXT_PANE);
  ASSERT_TRUE(WaitForFocus(true, web_app_menu_button_));
}

// Tests the app icon and title are not shown.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       IconShownAndTitleNotShown) {
  SetUpWebApp();
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_FALSE(browser_view->ShouldShowWindowIcon());
  EXPECT_FALSE(browser_view->ShouldShowWindowTitle());
}

// Tests that the custom tab bar is focusable from the keyboard.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       CustomTabBarIsFocusable) {
  SetUpWebApp();
  ASSERT_TRUE(WaitForFocus(true, browser_view_->contents_web_view()));

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(app_browser_);

  const GURL kOutOfScopeURL("http://example.org/");
  NavigateParams nav_params(app_browser_, kOutOfScopeURL,
                            ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&nav_params);
  auto* custom_tab_bar = browser_view->toolbar()->custom_tab_bar();

  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_NEXT_PANE);
  ASSERT_TRUE(WaitForFocus(true, web_app_menu_button_));

  EXPECT_FALSE(custom_tab_bar->close_button_for_testing()->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_NEXT_PANE);
  ASSERT_TRUE(WaitForFocus(true, custom_tab_bar->close_button_for_testing()));
}

// Tests that the focus previous pane command focuses the app menu for web-app
// windows.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       BrowserCommandFocusPreviousPane) {
  SetUpWebApp();
  ASSERT_TRUE(WaitForFocus(true, browser_view_->contents_web_view()));
  EXPECT_FALSE(web_app_menu_button_->HasFocus());
  chrome::ExecuteCommand(app_browser_, IDC_FOCUS_PREVIOUS_PANE);
  ASSERT_TRUE(WaitForFocus(true, web_app_menu_button_));
}

// Tests that a web app's content settings icons can be interacted with.
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       ContentSettingIcons) {
  SetUpWebApp();
  for (ContentSettingImageView* view : *content_setting_views_) {
    EXPECT_FALSE(view->GetVisible());
  }

  ContentSettingImageView* geolocation_icon = GrantGeolocationPermission();

  for (ContentSettingImageView* view : *content_setting_views_) {
    bool is_geolocation_icon = view == geolocation_icon;
    EXPECT_EQ(is_geolocation_icon, view->GetVisible());
  }
  EXPECT_FALSE(geolocation_icon->IsBubbleShowing());

  // Press the geolocation button.
  geolocation_icon->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE, ui::EF_NONE));
  geolocation_icon->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_SPACE, ui::EF_NONE));
  EXPECT_TRUE(geolocation_icon->IsBubbleShowing());
}

// Regression test for https://crbug.com/839955
IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       ActiveStateOfButtonMatchesWidget) {
  SetUpWebApp();
  chromeos::FrameCaptionButtonContainerView::TestApi test(
      GetFrameViewChromeOS(browser_view_)->caption_button_container());

  EXPECT_TRUE(WaitForPaintAsActive(true, test.size_button()));
  EXPECT_TRUE(GetPaintingAsActive());

  DeactivateWidget(browser_view_->GetWidget());
  EXPECT_TRUE(WaitForPaintAsActive(false, test.size_button()));
  EXPECT_FALSE(GetPaintingAsActive());
}

IN_PROC_BROWSER_TEST_P(WebAppNonClientFrameViewChromeOSTest,
                       PopupHasNoToolbar) {
  SetUpWebApp();

  Browser* popup_browser;
  {
    NavigateParams navigate_params(app_browser_, GetAppURL(),
                                   ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition = WindowOpenDisposition::NEW_POPUP;

    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    Navigate(&navigate_params);
    navigation_observer.WaitForNavigationFinished();
    popup_browser = navigate_params.browser;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(popup_browser);
  EXPECT_FALSE(browser_view->web_app_frame_toolbar_for_testing() &&
               browser_view->web_app_frame_toolbar_for_testing()->GetVisible());
}

// Test the normal type browser's kTopViewInset is always 0.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest, TopViewInset) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  aura::Window* window = browser()->window()->GetNativeWindow();
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset should be 0 when in immersive mode.
  EnterImmersiveFullscreenMode(browser());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // An immersive reveal shows the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // End the reveal and exit immersive mode.
  // The kTopViewInset should be 0 when immersive mode is exited.
  revealed_lock.reset();
  ExitImmersiveFullscreenMode(browser());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));
}

// Test that for a browser window, its caption buttons are always hidden in
// tablet mode.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       BrowserHeaderVisibilityInTabletModeTest) {
  if (!IsSnapWindowSupported()) {
    GTEST_SKIP() << "Ash is too old.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize |
          aura::client::kResizeBehaviorCanResize);

  // Caption buttons are not supported when using the WebUI tab strip.
  if (browser_view->webui_tab_strip()) {
    EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  } else {
    EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  }

  // Ensure the current layout is finished before entering overview mode.
  views::test::RunScheduledLayout(browser_view);
  ASSERT_FALSE(browser_view->needs_layout());

  EnterOverviewMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // b/362835104: Browser windows that are not web apps do not require a layout
  // after the header is made invisible. This is an optimization with no visual
  // impact. Depends on `kOverviewSessionInitOptimizations` being enabled.
  //
  // Lacros builds require a layout of the browser view for unrelated reasons.
  ASSERT_FALSE(browser_view->GetIsWebAppType());
  EXPECT_FALSE(browser_view->needs_layout());
#endif

  ExitOverviewMode();

  // Caption buttons are not supported when using the WebUI tab strip.
  if (browser_view->webui_tab_strip()) {
    EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  } else {
    EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  }

  EnterTabletMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  EnterOverviewMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  ExitOverviewMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  SnapWindow(widget->GetNativeWindow(), crosapi::mojom::SnapPosition::kPrimary);
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
}

// Regression test for https://crbug.com/879851.
// Tests that we don't accidentally change the color of app frame title bars.
// Update expectation if change is intentional.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest, AppFrameColor) {
  Browser* app_browser =
      CreateBrowserForApp("test_browser_app", browser()->profile());

  aura::Window* window = app_browser->window()->GetNativeWindow();
  SkColor active_frame_color =
      window->GetProperty(chromeos::kFrameActiveColorKey);

  const bool is_dark_mode_state =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetNativeTheme()
          ->ShouldUseDarkColors();
  EXPECT_EQ(active_frame_color, is_dark_mode_state
                                    ? gfx::kGoogleGrey900
                                    : SkColorSetRGB(0xFF, 0xFF, 0xFF))
      << "RGB: " << SkColorGetR(active_frame_color) << ", "
      << SkColorGetG(active_frame_color) << ", "
      << SkColorGetB(active_frame_color);
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       AccessibleProperties) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);
  ui::AXNodeData data;

  frame_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kTitleBar, data.role);
}

namespace {

constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

}  // namespace

class PreventCloseBrowserNonClientFrameViewChromeOSTest
    : public PreventCloseTestBase {
 public:
  PreventCloseBrowserNonClientFrameViewChromeOSTest() = default;

  PreventCloseBrowserNonClientFrameViewChromeOSTest(
      const PreventCloseBrowserNonClientFrameViewChromeOSTest&) = delete;
  PreventCloseBrowserNonClientFrameViewChromeOSTest& operator=(
      const PreventCloseBrowserNonClientFrameViewChromeOSTest&) = delete;

  ~PreventCloseBrowserNonClientFrameViewChromeOSTest() override = default;

  void SetUpOnMainThread() override {
    PreventCloseTestBase::SetUpOnMainThread();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    loopback_crosapi_ =
        std::make_unique<web_app::LoopbackCrosapiAppServiceProxy>(profile());
#endif
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    loopback_crosapi_ = nullptr;
#endif

    PreventCloseTestBase::TearDownOnMainThread();
  }

  views::Button* GetWindowCloseButton(Browser* browser) {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    auto* const frame_view =
        ChromeOSBrowserUITest::GetFrameViewChromeOS(browser_view);

    chromeos::FrameCaptionButtonContainerView::TestApi test_api(
        frame_view->caption_button_container());
    return test_api.close_button();
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<web_app::LoopbackCrosapiAppServiceProxy> loopback_crosapi_;
#endif
};

IN_PROC_BROWSER_TEST_F(PreventCloseBrowserNonClientFrameViewChromeOSTest,
                       CloseButtonIsDisabled) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(web_app::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  {
    auto* const close_button = GetWindowCloseButton(browser);
    ASSERT_TRUE(close_button);
    EXPECT_FALSE(close_button->GetEnabled());
  }

  {
    apps::AppUpdateWaiter waiter(
        browser->profile(), web_app::kCalculatorAppId,
        base::BindRepeating([](const apps::AppUpdate& update) {
          return update.AllowClose().has_value() && update.AllowClose().value();
        }));
    ClearWebAppSettings();
    waiter.Await();
  }

  {
    auto* const close_button = GetWindowCloseButton(browser);
    ASSERT_TRUE(close_button);
    EXPECT_TRUE(close_button->GetEnabled());
  }
}

IN_PROC_BROWSER_TEST_F(PreventCloseBrowserNonClientFrameViewChromeOSTest,
                       CloseButtonIsEnabled) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  auto* const close_button = GetWindowCloseButton(browser);
  ASSERT_TRUE(close_button);
  EXPECT_TRUE(close_button->GetEnabled());

  ClearWebAppSettings();
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       ImmersiveModeTopViewInset) {
  Browser* app_browser =
      CreateBrowserForApp("test_browser_app", browser()->profile());
  // TODO(neis): Move this into the CreateBrowser* functions.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ui_test_utils::CreateAsyncWidgetRequestWaiter(*browser()).Wait();
#endif

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  aura::Window* window = app_browser->window()->GetNativeWindow();
  EXPECT_LT(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset should be 0 when in immersive mode.
  EnterImmersiveFullscreenMode(app_browser);
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // An immersive reveal shows the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // End the reveal and exit immersive mode.
  // The kTopViewInset should be larger than 0 again when immersive mode is
  // exited.
  revealed_lock.reset();
  ExitImmersiveFullscreenMode(app_browser);
  EXPECT_LT(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset is the same as in overview mode.
  const int inset_normal = window->GetProperty(aura::client::kTopViewInset);
  EnterOverviewMode();
  const int inset_in_overview_mode =
      window->GetProperty(aura::client::kTopViewInset);
  EXPECT_EQ(inset_normal, inset_in_overview_mode);
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       ToggleTabletModeWhileImmersiveModeEnabled) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();

  EnterImmersiveFullscreenMode(browser());

  // Should exit immersive mode + fullscreen when tablet mode is enabled.
  EnterTabletMode();
  ImmersiveModeTester(browser()).WaitForFullscreenToExit();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_FALSE(browser_view->IsFullscreen());
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewChromeOSTest,
                       ToggleImmersiveModeWhileTabletModeEnabled) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  ASSERT_FALSE(immersive_mode_controller->IsEnabled());
  ASSERT_FALSE(browser_view->IsFullscreen());

  EnterTabletMode();

  // Should be able to enter immersive mode even when the tablet mode is
  // enabled.
  EnterImmersiveFullscreenMode(browser());
}

// TODO(b/270175923): Consider using WebUiTabStripOverrideTest, since it
// makes sense for it to always be enabled.
using FloatBrowserNonClientFrameViewChromeOSTest =
    TopChromeMdParamTest<ChromeOSBrowserUITest>;

// TODO(crbug.com/40286309): Port this test to Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(FloatBrowserNonClientFrameViewChromeOSTest,
                       TabletModeMultitaskMenu) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  EnterTabletMode();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  aura::Window* window = widget->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow());

  // A normal tap on the top center of the window and in the omnibox
  // bounds will focus the omnibox.
  views::View* omnibox = browser_view->GetViewByID(VIEW_ID_OMNIBOX);
  const gfx::Rect omnibox_bounds = omnibox->GetBoundsInScreen();
  ASSERT_NO_FATAL_FAILURE(
      event_generator.GestureTapAt(omnibox_bounds.top_center()));
  ASSERT_TRUE(WaitForFocus(true, omnibox));

  // Swipe down from the top center opens the multitask menu.
  event_generator.SetTouchRadius(10, 5);
  const gfx::Point top_center(window->bounds().CenterPoint().x(), -1);
  event_generator.PressTouch(top_center);
  event_generator.MoveTouchBy(0, 100);
  event_generator.ReleaseTouch();
  ASSERT_TRUE(WaitForFocus(false, omnibox));
  auto* multitask_menu_event_handler =
      ash::TabletModeControllerTestApi()
          .tablet_mode_window_manager()
          ->tablet_mode_multitask_menu_controller();
  EXPECT_TRUE(multitask_menu_event_handler->multitask_menu());

  if (browser_view->webui_tab_strip()) {
    // The tab strip doesn't get shown if the menu is.
    ASSERT_FALSE(browser_view->webui_tab_strip()->GetVisible());
  }

  // Tap on the omnibox outside the menu takes focus and closes the menu.
  ASSERT_NO_FATAL_FAILURE(
      event_generator.GestureTapAt(omnibox_bounds.left_center()));
  ASSERT_TRUE(WaitForFocus(true, omnibox));
  EXPECT_FALSE(multitask_menu_event_handler->multitask_menu());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(FloatBrowserNonClientFrameViewChromeOSTest,
                       BrowserHeaderVisibilityInTabletModeTest) {
  if (!IsSnapWindowSupported()) {
    GTEST_SKIP() << "Ash is too old.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

  EnterTabletMode();
  ASSERT_TRUE(WaitForVisible(false, frame_view->caption_button_container()));

  aura::Window* window = browser_view->GetWidget()->GetNativeWindow();
  auto* immersive_controller = chromeos::ImmersiveFullscreenController::Get(
      views::Widget::GetWidgetForNativeView(window));

  // Snap the window. No immersive mode from regular browsers.
  SnapWindow(window, crosapi::mojom::SnapPosition::kSecondary);
  ASSERT_TRUE(WaitForVisible(false, frame_view->caption_button_container()));
  EXPECT_FALSE(immersive_controller->IsEnabled());

  // Float the window; the title bar becomes visible.
  chromeos::FloatControllerBase::Get()->SetFloat(
      window, chromeos::FloatStartLocation::kBottomRight);
  ASSERT_TRUE(WaitForVisible(true, frame_view->caption_button_container()));
  EXPECT_FALSE(immersive_controller->IsEnabled());
}

// Test that for a browser app window, its caption buttons may or may not hide
// in tablet mode.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40946296): Finish porting to Lacros when the bug is fixed.
#define MAYBE_BrowserAppHeaderVisibilityInTabletModeTest \
  DISABLED_BrowserAppHeaderVisibilityInTabletModeTest
#else
#define MAYBE_BrowserAppHeaderVisibilityInTabletModeTest \
  BrowserAppHeaderVisibilityInTabletModeTest
#endif
IN_PROC_BROWSER_TEST_P(FloatBrowserNonClientFrameViewChromeOSTest,
                       MAYBE_BrowserAppHeaderVisibilityInTabletModeTest) {
  if (!IsSnapWindowSupported()) {
    GTEST_SKIP() << "Ash is too old.";
  }

  Browser* browser2 =
      CreateBrowserForApp("test_browser_app", browser()->profile());
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  views::Widget* widget2 = browser_view2->GetWidget();
  BrowserNonClientFrameViewChromeOS* frame_view2 =
      GetFrameViewChromeOS(browser_view2);
  widget2->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize |
          aura::client::kResizeBehaviorCanResize);

  EnterOverviewMode();
  EXPECT_FALSE(frame_view2->caption_button_container()->GetVisible());
  ExitOverviewMode();
  EXPECT_TRUE(frame_view2->caption_button_container()->GetVisible());

  EnterTabletMode();
  EnterOverviewMode();
  EXPECT_FALSE(frame_view2->caption_button_container()->GetVisible());
  ExitOverviewMode();
  EXPECT_TRUE(frame_view2->caption_button_container()->GetVisible());

  auto* immersive_controller = chromeos::ImmersiveFullscreenController::Get(
      views::Widget::GetWidgetForNativeView(widget2->GetNativeWindow()));
  EXPECT_TRUE(immersive_controller->IsEnabled());

  // Snap a window. Immersive mode is enabled so its title bar is not visible.
  SnapWindow(widget2->GetNativeWindow(),
             crosapi::mojom::SnapPosition::kSecondary);
  EXPECT_TRUE(frame_view2->caption_button_container()->GetVisible());
  EXPECT_TRUE(immersive_controller->IsEnabled());

  // Float a window. Immersive mode is disabled so its title bar is visible.
  ui::test::EventGenerator event_generator(
      widget2->GetNativeWindow()->GetRootWindow());
  event_generator.PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(frame_view2->caption_button_container()->GetVisible());
  EXPECT_FALSE(immersive_controller->IsEnabled());
}

// Tests that, with the float flag enabled, the accelerator toggles the
// multitask menu on a browser window.
IN_PROC_BROWSER_TEST_P(FloatBrowserNonClientFrameViewChromeOSTest,
                       ToggleMultitaskMenu) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_TOGGLE_MULTITASK_MENU));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);
  auto* size_button = static_cast<chromeos::FrameSizeButton*>(
      frame_view->caption_button_container()->size_button());

  // Pressing accelerator once should show the multitask menu.
  ui::test::EventGenerator event_generator(
      browser_view->GetWidget()->GetNativeWindow()->GetRootWindow());
  event_generator.PressAndReleaseKeyAndModifierKeys(ui::VKEY_Z,
                                                    ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return size_button->IsMultitaskMenuShown(); }));

  // With platform bubble, key event is routed to the platform bubble at ozone
  // level, so dispatch it to the multitask_menu_widget directly.
  if (views::test::IsOzoneBubblesUsingPlatformWidgets()) {
    ui::test::EventGenerator multitask_view_event_generator(
        size_button->multitask_menu_widget_for_testing()
            ->GetNativeWindow()
            ->GetRootWindow());
    // Pressing accelerator a second time should close the menu.
    multitask_view_event_generator.PressAndReleaseKeyAndModifierKeys(
        ui::VKEY_Z, ui::EF_COMMAND_DOWN);
  } else {
    // Pressing accelerator a second time should close the menu.
    event_generator.PressAndReleaseKeyAndModifierKeys(ui::VKEY_Z,
                                                      ui::EF_COMMAND_DOWN);
  }

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !size_button->IsMultitaskMenuShown(); }));
}

using HomeLauncherBrowserNonClientFrameViewChromeOSTest =
    BrowserNonClientFrameViewChromeOSTest;

IN_PROC_BROWSER_TEST_P(HomeLauncherBrowserNonClientFrameViewChromeOSTest,
                       TabletModeBrowserCaptionButtonVisibility) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

  // Caption buttons are not supported when using the WebUI tab strip.
  if (browser_view->webui_tab_strip()) {
    EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  } else {
    EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  }

  EnterTabletMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());

  EnterOverviewMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  ExitOverviewMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());

  ExitTabletMode();

  // Caption buttons are not supported when using the WebUI tab strip.
  if (browser_view->webui_tab_strip()) {
    EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  } else {
    EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  }
}

// TODO(crbug.com/40640473): When the test flake has been addressed, improve
// performance by consolidating this unit test with
// |TabletModeBrowserCaptionButtonVisibility|. Do not forget to remove the
// corresponding |FRIEND_TEST_ALL_PREFIXES| usage from
// |BrowserNonClientFrameViewChromeOS|.
IN_PROC_BROWSER_TEST_P(HomeLauncherBrowserNonClientFrameViewChromeOSTest,
                       CaptionButtonVisibilityForBrowserLaunchedInTabletMode) {
  EnterTabletMode();
  auto* new_browser = CreateBrowser(browser()->profile());
  auto* frame_view =
      GetFrameViewChromeOS(BrowserView::GetBrowserViewForBrowser(new_browser));
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40946296): Finish porting to Lacros when the bug is fixed.
#define MAYBE_TabletModeAppCaptionButtonVisibility \
  DISABLED_TabletModeAppCaptionButtonVisibility
#else
#define MAYBE_TabletModeAppCaptionButtonVisibility \
  TabletModeAppCaptionButtonVisibility
#endif
IN_PROC_BROWSER_TEST_P(HomeLauncherBrowserNonClientFrameViewChromeOSTest,
                       MAYBE_TabletModeAppCaptionButtonVisibility) {
  Browser* app_browser =
      CreateBrowserForApp("test_browser_app", browser()->profile());
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);
  EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Tablet mode doesn't affect app's caption button's visibility.
  EnterTabletMode();
  EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());

  // However, overview mode does.
  EnterOverviewMode();
  EXPECT_FALSE(frame_view->caption_button_container()->GetVisible());
  ExitOverviewMode();
  EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());

  ExitTabletMode();
  EXPECT_TRUE(frame_view->caption_button_container()->GetVisible());
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
}

using LockedFullscreenBrowserNonClientFrameViewChromeOSTest =
    TopChromeMdParamTest<ChromeOSBrowserUITest>;

IN_PROC_BROWSER_TEST_P(LockedFullscreenBrowserNonClientFrameViewChromeOSTest,
                       ToggleTabletMode) {
  if (!IsIsShelfVisibleSupported()) {
    GTEST_SKIP() << "Ash is too old.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());

  // Set locked fullscreen state.
  PinWindow(browser_view->GetWidget()->GetNativeWindow(), /*trusted=*/true);

  // We're fullscreen, immersive is disabled in locked fullscreen, and while
  // we're at it, also make sure that the shelf is hidden.
  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40276379): Enable this assertion once the bug is fixed (at
  // the moment PinWindow returns too early).
#else
  EXPECT_FALSE(IsShelfVisible());
#endif

  auto* widget = browser_view->GetWidget();
  auto* immersive_controller = chromeos::ImmersiveFullscreenController::Get(
      views::Widget::GetWidgetForNativeView(widget->GetNativeWindow()));
  EXPECT_FALSE(immersive_controller->IsEnabled());

  EnterTabletMode();

  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());
}

// The remaining tests make sense only for Ash, not for Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)

using BrowserNonClientFrameViewAshTestNoWebUiTabStrip =
    BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip;

// Tests that Avatar icon should show on the top left corner of the teleported
// browser window on ChromeOS.
// TODO(http://crbug.com/1059514): This test should be made to work with the
// webUI tabstrip.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTestNoWebUiTabStrip,
                       AvatarDisplayOnTeleportedWindow) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);
  BrowserNonClientFrameViewChromeOSTestApi test_api(frame_view);
  aura::Window* window = browser()->window()->GetNativeWindow();

  EXPECT_FALSE(MultiUserWindowManagerHelper::ShouldShowAvatar(window));
  EXPECT_FALSE(test_api.GetProfileIndicatorIcon());

  const AccountId account_id1 =
      multi_user_util::GetAccountIdFromProfile(browser()->profile());
  TestMultiUserWindowManager* window_manager =
      TestMultiUserWindowManager::Create(browser(), account_id1);

  // Teleport the window to another desktop.
  const AccountId account_id2(AccountId::FromUserEmail("user2"));
  window_manager->ShowWindowForUser(window, account_id2);
  EXPECT_TRUE(MultiUserWindowManagerHelper::ShouldShowAvatar(window));
  EXPECT_TRUE(test_api.GetProfileIndicatorIcon());

  // Teleport the window back to owner desktop.
  window_manager->ShowWindowForUser(window, account_id1);
  EXPECT_FALSE(MultiUserWindowManagerHelper::ShouldShowAvatar(window));
  EXPECT_FALSE(test_api.GetProfileIndicatorIcon());
}

using BrowserNonClientFrameViewAshTest = BrowserNonClientFrameViewChromeOSTest;

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       SettingsSystemWebAppHasMinimumWindowSize) {
  // Install the Settings System Web App.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  // Open a settings window.
  ui_test_utils::BrowserChangeObserver observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  auto* settings_manager = chrome::SettingsWindowManager::GetInstance();
  settings_manager->ShowOSSettings(browser()->profile());
  observer.Wait();

  Browser* settings_browser =
      settings_manager->FindBrowserForProfile(browser()->profile());

  // Try to set the bounds to a tiny value.
  settings_browser->window()->SetBounds(gfx::Rect(1, 1));

  // The window has a reasonable size.
  gfx::Rect actual_bounds = settings_browser->window()->GetBounds();
  EXPECT_LE(300, actual_bounds.width());
  EXPECT_LE(100, actual_bounds.height());
}

// Enumeration of test modes for
// `BrowserNonClientFrameViewAshThemeChangeTest`s.
enum class ThemeChangeTestMode {
  kSWA,
  kNonSWA,
};

// Base class for `ThemeChange` tests, parameterized by test mode and:
// * whether manifest background color is preferred,
// * whether theme changes should be animated.
class BrowserNonClientFrameViewAshThemeChangeTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ThemeChangeTestMode,
                     /*should_animate_theme_changes=*/bool>> {
 public:
  BrowserNonClientFrameViewAshThemeChangeTest() {
    switch (GetThemeChangeTestMode()) {
      case ThemeChangeTestMode::kSWA: {
        system_web_app_installation_ =
            ash::TestSystemWebAppInstallation::SetUpAppWithColors(
                /*theme_color=*/SK_ColorWHITE,
                /*dark_mode_theme_color=*/SK_ColorBLACK,
                /*background_color=*/SK_ColorWHITE,
                /*dark_mode_background_color=*/SK_ColorBLACK);
        auto* delegate = static_cast<ash::UnittestingSystemAppDelegate*>(
            system_web_app_installation_->GetDelegate());
        delegate->SetShouldAnimateThemeChanges(ShouldAnimateThemeChanges());
        // When system colored SWAs were introduced for Jelly,
        // `UseSystemThemeColor()` overrode other styling information in the
        // manifest. This test now verifies behavior for SWAs that are opted out
        // of the system styling (by setting it to false).
        delegate->SetUseSystemThemeColor(false);
        break;
      }
      case ThemeChangeTestMode::kNonSWA:
        break;
    }
  }

  // Returns test mode given test parameterization.
  ThemeChangeTestMode GetThemeChangeTestMode() const {
    return std::get<0>(GetParam());
  }

  // Returns whether theme changes should be animated for the web app under test
  // given test parameterization.
  bool ShouldAnimateThemeChanges() const { return std::get<1>(GetParam()); }

  // Toggles the color mode, triggering propagation of theme change events.
  void ToggleColorMode() {
    auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

    native_theme->set_use_dark_colors(!native_theme->ShouldUseDarkColors());
    native_theme->set_preferred_color_scheme(
        native_theme->CalculatePreferredColorScheme());

    native_theme->NotifyOnNativeThemeUpdated();
  }

  // Installs the web app under test, blocking until installation is complete,
  // and returning the `webapps::AppId` for the installed web app.
  webapps::AppId WaitForAppInstall() {
    switch (GetThemeChangeTestMode()) {
      case ThemeChangeTestMode::kSWA:
        system_web_app_installation_->WaitForAppInstall();
        return system_web_app_installation_->GetAppId();
      case ThemeChangeTestMode::kNonSWA: {
        if (!test_server_) {
          test_server_ = std::make_unique<net::EmbeddedTestServer>(
              net::EmbeddedTestServer::TYPE_HTTPS);
          test_server_->AddDefaultHandlers(GetChromeTestDataDir());
          CHECK(test_server_->Start());
        }
        const GURL app_url =
            test_server_->GetURL("app.com", "/ssl/google.html");
        auto web_app_info =
            web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
        web_app_info->scope = app_url.GetWithoutFilename();
        web_app_info->theme_color = SK_ColorWHITE;
        web_app_info->dark_mode_theme_color = SK_ColorBLACK;
        web_app_info->background_color = SK_ColorWHITE;
        web_app_info->dark_mode_background_color = SK_ColorBLACK;
        return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
      }
    }
  }

  // Returns the `Profile` associated with the web app under test.
  Profile* profile() { return browser()->profile(); }

 private:
  std::unique_ptr<ash::TestSystemWebAppInstallation>
      system_web_app_installation_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

INSTANTIATE_TEST_SUITE_P(
    Mode,
    BrowserNonClientFrameViewAshThemeChangeTest,
    testing::Combine(testing::Values(ThemeChangeTestMode::kSWA,
                                     ThemeChangeTestMode::kNonSWA),
                     /*should_animate_theme_changes=*/testing::Bool()),
    [](const testing::TestParamInfo<
        std::tuple<ThemeChangeTestMode,
                   /*should_animate_theme_changes=*/bool>>& info) {
      ThemeChangeTestMode test_mode = std::get<0>(info.param);
      bool should_animate_theme_changes = std::get<1>(info.param);

      std::stringstream name;
      switch (test_mode) {
        case ThemeChangeTestMode::kSWA:
          name << "kSWA";
          break;
        case ThemeChangeTestMode::kNonSWA:
          name << "kNonSWA";
          break;
      }

      if (should_animate_theme_changes) {
        name << "_ShouldAnimateThemeChanges";
      }

      return name.str();
    });

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshThemeChangeTest,
                       ThemeChange) {
  // Skip test parameterizations for non-system web apps that don't make sense.
  if (GetThemeChangeTestMode() == ThemeChangeTestMode::kNonSWA &&
      ShouldAnimateThemeChanges()) {
    GTEST_SKIP();
  }

  const webapps::AppId app_id = WaitForAppInstall();

  // Trigger the launch but do not wait for the web contents to load.
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              app_id, apps::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::CURRENT_TAB,
              apps::LaunchSource::kFromTest));
  ASSERT_TRUE(web_contents);
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  auto* contents_web_view =
      BrowserView::GetBrowserViewForBrowser(browser)->contents_web_view();

  // Verify background color is immediately resolved from the app controller
  // despite the fact that the web contents background color hasn't loaded
  // yet.
  EXPECT_EQ(contents_web_view->GetBackground()->get_color(),
            browser->app_controller()->GetBackgroundColor().value());
  EXPECT_FALSE(web_contents->GetBackgroundColor().has_value());

  // Wait for the web contents background color to load and verify that the
  // background color still matches that resolved from the app controller.
  {
    content::BackgroundColorChangeWaiter waiter(web_contents);
    waiter.Wait();
    EXPECT_EQ(contents_web_view->GetBackground()->get_color(),
              browser->app_controller()->GetBackgroundColor().value());
    EXPECT_EQ(contents_web_view->GetBackground()->get_color(),
              web_contents->GetBackgroundColor().value());
  }

  content::AwaitDocumentOnLoadCompleted(web_contents);

  // Toggle color mode and verify background color is immediately resolved
  // from the app controller. If a system web app is loaded which prefers
  // manifest colors, there will be a temporary mismatch between the contents
  // background color and the web contents background color due to the fact
  // that the web contents background color update is async.
  ToggleColorMode();
  EXPECT_EQ(contents_web_view->GetBackground()->get_color(),
            browser->app_controller()->GetBackgroundColor().value());
  EXPECT_EQ(contents_web_view->GetBackground()->get_color(),
            web_contents->GetBackgroundColor().value());

  // If theme changes should be animated, the layer associated with the
  // `contents_web_view` native view should be immediately hidden.
  auto* layer = contents_web_view->holder()->native_view()->layer();
  if (ShouldAnimateThemeChanges()) {
    EXPECT_EQ(layer->GetTargetOpacity(), std::nextafter(0.f, 1.f));
  } else {
    EXPECT_EQ(layer->GetTargetOpacity(), 1.f);
  }

  // Wait for the web contents background color to update and verify that the
  // background color still matches that resolved from the app controller.
  {
    content::BackgroundColorChangeWaiter waiter(web_contents);
    waiter.Wait();
    EXPECT_EQ(contents_web_view->GetBackground()->get_color(),
              browser->app_controller()->GetBackgroundColor().value());
    EXPECT_EQ(contents_web_view->GetBackground()->get_color(),
              web_contents->GetBackgroundColor().value());
  }

  // If theme changes should be animated, the layer associated with the
  // `contents_web_view` native view should be animated back in only after a
  // round trip through the renderer and compositor pipelines. This should
  // ensure that the web contents has finished repainting theme changes.
  if (ShouldAnimateThemeChanges()) {
    base::test::TestFuture<bool> visual_state_change_future;
    web_contents->GetRenderViewHost()->GetWidget()->InsertVisualStateCallback(
        visual_state_change_future.GetCallback());
    EXPECT_TRUE(visual_state_change_future.Wait());
    EXPECT_EQ(layer->GetTargetOpacity(), 1.f);
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#define INSTANTIATE_TEST_SUITE(name) \
  INSTANTIATE_TEST_SUITE_P(All, name, ::testing::Values(false, true))

INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewChromeOSTest);
INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip);
INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewChromeOSTestWithWebUiTabStrip);
INSTANTIATE_TEST_SUITE(FloatBrowserNonClientFrameViewChromeOSTest);
INSTANTIATE_TEST_SUITE(HomeLauncherBrowserNonClientFrameViewChromeOSTest);
INSTANTIATE_TEST_SUITE(LockedFullscreenBrowserNonClientFrameViewChromeOSTest);
INSTANTIATE_TEST_SUITE(WebAppNonClientFrameViewChromeOSTest);

#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewAshTestNoWebUiTabStrip);
INSTANTIATE_TEST_SUITE(BrowserNonClientFrameViewAshTest);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
