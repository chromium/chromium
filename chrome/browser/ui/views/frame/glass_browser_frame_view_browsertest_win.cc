// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include <tuple>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/glass_browser_caption_button_container.h"
#include "chrome/browser/ui/views/frame/windows_caption_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/view_utils.h"

class GlassBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  GlassBrowserFrameViewTest() = default;
  GlassBrowserFrameViewTest(const GlassBrowserFrameViewTest&) = delete;
  GlassBrowserFrameViewTest& operator=(const GlassBrowserFrameViewTest&) =
      delete;
  ~GlassBrowserFrameViewTest() override = default;

 protected:
  GlassBrowserFrameView* GetGlassBrowserFrameView() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    views::NonClientFrameView* frame_view =
        browser_view->GetWidget()->non_client_view()->frame_view();

    if (!views::IsViewClass<GlassBrowserFrameView>(frame_view))
      return nullptr;
    return static_cast<GlassBrowserFrameView*>(frame_view);
  }

  const WindowsCaptionButton* GetMaximizeButton() {
    auto* glass_frame_view = GetGlassBrowserFrameView();
    if (!glass_frame_view)
      return nullptr;
    auto* caption_button_container =
        glass_frame_view->caption_button_container_for_testing();
    return static_cast<const WindowsCaptionButton*>(
        caption_button_container->GetViewByID(VIEW_ID_MAXIMIZE_BUTTON));
  }
};

// Test that in touch mode, the maximize button is enabled for a non-maximized
// window.
IN_PROC_BROWSER_TEST_F(GlassBrowserFrameViewTest,
                       NonMaximizedTouchMaximizeButtonState) {
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
  auto* maximize_button = GetMaximizeButton();
  if (!maximize_button)
    GTEST_SKIP();

  EXPECT_TRUE(maximize_button->GetVisible());
  EXPECT_TRUE(maximize_button->GetEnabled());
}

// Test that in touch mode, the maximize button is disabled and not visible for
// a maximized window.
IN_PROC_BROWSER_TEST_F(GlassBrowserFrameViewTest,
                       MaximizedTouchMaximizeButtonState) {
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
  auto* glass_frame_view = GetGlassBrowserFrameView();
  if (!glass_frame_view)
    GTEST_SKIP();

  glass_frame_view->frame()->Maximize();

  auto* maximize_button = GetMaximizeButton();

  // Button isn't visible, and should be disabled.
  EXPECT_FALSE(maximize_button->GetEnabled());
  EXPECT_FALSE(maximize_button->GetVisible());
}

// Test that in non touch mode, the maximize button is enabled for a
// non-maximized window.
IN_PROC_BROWSER_TEST_F(GlassBrowserFrameViewTest,
                       NonTouchNonMaximizedMaximizeButtonState) {
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{false};
  auto* maximize_button = GetMaximizeButton();
  if (!maximize_button)
    GTEST_SKIP();

  EXPECT_TRUE(maximize_button->GetVisible());
  EXPECT_TRUE(maximize_button->GetEnabled());
}

// Test that in non touch mode, the maximize button is enabled and not visible
// for a maximized window.
IN_PROC_BROWSER_TEST_F(GlassBrowserFrameViewTest,
                       NonTouchMaximizedMaximizeButtonState) {
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{false};
  auto* glass_frame_view = GetGlassBrowserFrameView();
  if (!glass_frame_view)
    GTEST_SKIP();

  glass_frame_view->frame()->Maximize();

  auto* maximize_button = GetMaximizeButton();
  EXPECT_FALSE(maximize_button->GetVisible());
  EXPECT_TRUE(maximize_button->GetEnabled());
}

class WebAppGlassBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  WebAppGlassBrowserFrameViewTest() = default;
  WebAppGlassBrowserFrameViewTest(const WebAppGlassBrowserFrameViewTest&) =
      delete;
  WebAppGlassBrowserFrameViewTest& operator=(
      const WebAppGlassBrowserFrameViewTest&) = delete;
  ~WebAppGlassBrowserFrameViewTest() override = default;

  GURL GetStartURL() { return GURL("https://test.org"); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    WebAppToolbarButtonContainer::DisableAnimationForTesting();
  }

  // Windows 7 does not use GlassBrowserFrameView when Aero glass is not
  // enabled. Skip testing in this scenario.
  // TODO(https://crbug.com/863278): Force Aero glass on Windows 7 for this
  // test.
  bool InstallAndLaunchWebApp() {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = GetStartURL();
    web_app_info->scope = GetStartURL().GetWithoutFilename();
    if (theme_color_)
      web_app_info->theme_color = *theme_color_;

    web_app::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetStartURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    if (!views::IsViewClass<GlassBrowserFrameView>(frame_view))
      return false;
    glass_frame_view_ = static_cast<GlassBrowserFrameView*>(frame_view);

    web_app_frame_toolbar_ = browser_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());
    return true;
  }

  absl::optional<SkColor> theme_color_ = SK_ColorBLUE;
  raw_ptr<Browser, DanglingUntriaged> app_browser_ = nullptr;
  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<GlassBrowserFrameView, DanglingUntriaged> glass_frame_view_ = nullptr;
  raw_ptr<WebAppFrameToolbarView, DanglingUntriaged> web_app_frame_toolbar_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, ThemeColor) {
  if (!InstallAndLaunchWebApp())
    return;

  EXPECT_EQ(glass_frame_view_->GetTitlebarColor(), *theme_color_);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, NoThemeColor) {
  theme_color_ = absl::nullopt;
  if (!InstallAndLaunchWebApp())
    return;

  EXPECT_EQ(
      glass_frame_view_->GetTitlebarColor(),
      browser()->window()->GetColorProvider()->GetColor(ui::kColorFrameActive));
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, MaximizedLayout) {
  if (!InstallAndLaunchWebApp())
    return;

  glass_frame_view_->frame()->Maximize();
  RunScheduledLayouts();

  views::View* const window_title =
      glass_frame_view_->GetViewByID(VIEW_ID_WINDOW_TITLE);
  DCHECK_GT(window_title->x(), 0);
  DCHECK_GE(web_app_frame_toolbar_->y(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, RTLTopRightHitTest) {
  base::i18n::SetRTLForTesting(true);
  if (!InstallAndLaunchWebApp())
    return;

  RunScheduledLayouts();

  // Avoid the top right resize corner.
  constexpr int kInset = 10;
  EXPECT_EQ(glass_frame_view_->NonClientHitTest(
                gfx::Point(glass_frame_view_->width() - kInset, kInset)),
            HTCAPTION);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, Fullscreen) {
  if (!InstallAndLaunchWebApp())
    return;

  glass_frame_view_->frame()->SetFullscreen(true);
  browser_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Verify that all children except the ClientView are hidden when the window
  // is fullscreened.
  for (views::View* child : glass_frame_view_->children()) {
    EXPECT_EQ(views::IsViewClass<views::ClientView>(child),
              child->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, ContainerHeight) {
  if (!InstallAndLaunchWebApp())
    return;

  static_cast<views::View*>(glass_frame_view_)
      ->GetWidget()
      ->LayoutRootViewIfNecessary();

  EXPECT_EQ(
      web_app_frame_toolbar_->height(),
      glass_frame_view_->caption_button_container_for_testing()->height());

  glass_frame_view_->frame()->Maximize();

  EXPECT_EQ(
      web_app_frame_toolbar_->height(),
      glass_frame_view_->caption_button_container_for_testing()->height());
}

class WebAppGlassBrowserFrameViewWindowControlsOverlayTest
    : public InProcessBrowserTest {
 public:
  WebAppGlassBrowserFrameViewWindowControlsOverlayTest() = default;
  WebAppGlassBrowserFrameViewWindowControlsOverlayTest(
      const WebAppGlassBrowserFrameViewWindowControlsOverlayTest&) = delete;
  WebAppGlassBrowserFrameViewWindowControlsOverlayTest& operator=(
      const WebAppGlassBrowserFrameViewWindowControlsOverlayTest&) = delete;

  ~WebAppGlassBrowserFrameViewWindowControlsOverlayTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  bool InstallAndLaunchWebAppWithWindowControlsOverlay() {
    GURL start_url = web_app_frame_toolbar_helper_
                         .LoadWindowControlsOverlayTestPageWithDataAndGetURL(
                             embedded_test_server(), &temp_dir_);

    std::vector<blink::mojom::DisplayMode> display_overrides = {
        blink::mojom::DisplayMode::kWindowControlsOverlay};
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    web_app_info->display_override = display_overrides;

    web_app::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));

    content::TestNavigationObserver navigation_observer(start_url);
    base::RunLoop loop;
    navigation_observer.StartWatchingNewWebContents();
    Browser* app_browser =
        web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

    // TODO(crbug.com/1191186): Register binder for BrowserInterfaceBroker
    // during testing.
    app_browser->app_controller()->SetOnUpdateDraggableRegionForTesting(
        loop.QuitClosure());
    web_app::NavigateToURLAndWait(app_browser, start_url);
    loop.Run();
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    if (!views::IsViewClass<GlassBrowserFrameView>(frame_view))
      return false;

    glass_frame_view_ = static_cast<GlassBrowserFrameView*>(frame_view);
    auto* web_app_frame_toolbar =
        browser_view_->web_app_frame_toolbar_for_testing();

    DCHECK(web_app_frame_toolbar);
    DCHECK(web_app_frame_toolbar->GetVisible());
    return true;
  }

  void ToggleWindowControlsOverlayEnabledAndWait() {
    auto* web_contents = browser_view_->GetActiveWebContents();
    web_app_frame_toolbar_helper_.SetupGeometryChangeCallback(web_contents);
    base::test::TestFuture<void> future;
    browser_view_->ToggleWindowControlsOverlayEnabled(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<GlassBrowserFrameView, DanglingUntriaged> glass_frame_view_ = nullptr;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       ContainerHeight) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();

  EXPECT_EQ(
      browser_view_->web_app_frame_toolbar_for_testing()->height(),
      glass_frame_view_->caption_button_container_for_testing()->height());

  glass_frame_view_->frame()->Maximize();

  EXPECT_EQ(
      browser_view_->web_app_frame_toolbar_for_testing()->height(),
      glass_frame_view_->caption_button_container_for_testing()->height());
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       Fullscreen) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();

  EXPECT_GT(glass_frame_view_->GetBoundsForClientView().y(), 0);

  glass_frame_view_->frame()->SetFullscreen(true);
  browser_view_->GetWidget()->LayoutRootViewIfNecessary();

  // ClientView should be covering the entire screen.
  EXPECT_EQ(glass_frame_view_->GetBoundsForClientView().y(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       CaptionButtonsTooltip) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  auto* caption_button_container =
      glass_frame_view_->caption_button_container_for_testing();
  auto* minimize_button = static_cast<const WindowsCaptionButton*>(
      caption_button_container->GetViewByID(VIEW_ID_MINIMIZE_BUTTON));
  auto* maximize_button = static_cast<const WindowsCaptionButton*>(
      caption_button_container->GetViewByID(VIEW_ID_MAXIMIZE_BUTTON));
  auto* restore_button = static_cast<const WindowsCaptionButton*>(
      caption_button_container->GetViewByID(VIEW_ID_RESTORE_BUTTON));
  auto* close_button = static_cast<const WindowsCaptionButton*>(
      caption_button_container->GetViewByID(VIEW_ID_CLOSE_BUTTON));

  // Verify tooltip text was first empty.
  EXPECT_EQ(minimize_button->GetTooltipText(), u"");
  EXPECT_EQ(maximize_button->GetTooltipText(), u"");
  EXPECT_EQ(restore_button->GetTooltipText(), u"");
  EXPECT_EQ(close_button->GetTooltipText(), u"");

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify tooltip text has been updated.
  EXPECT_EQ(minimize_button->GetTooltipText(),
            minimize_button->GetAccessibleName());
  EXPECT_EQ(maximize_button->GetTooltipText(),
            maximize_button->GetAccessibleName());
  EXPECT_EQ(restore_button->GetTooltipText(),
            restore_button->GetAccessibleName());
  EXPECT_EQ(close_button->GetTooltipText(), close_button->GetAccessibleName());

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify tooltip text has been cleared when the feature is toggled off.
  EXPECT_EQ(minimize_button->GetTooltipText(), u"");
  EXPECT_EQ(maximize_button->GetTooltipText(), u"");
  EXPECT_EQ(restore_button->GetTooltipText(), u"");
  EXPECT_EQ(close_button->GetTooltipText(), u"");
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       CaptionButtonHitTest) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  glass_frame_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Avoid the top right resize corner.
  constexpr int kInset = 10;
  const gfx::Point kPoint(glass_frame_view_->width() - kInset, kInset);

  EXPECT_EQ(glass_frame_view_->NonClientHitTest(kPoint), HTCLOSE);

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify the component updates on toggle.
  EXPECT_EQ(glass_frame_view_->NonClientHitTest(kPoint), HTCLIENT);

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify the component clears when the feature is turned off.
  EXPECT_EQ(glass_frame_view_->NonClientHitTest(kPoint), HTCLOSE);
}

// Regression test for https://crbug.com/1286896.
IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       TitlebarLayoutAfterUpdateWindowTitle) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();
  glass_frame_view_->GetWidget()->LayoutRootViewIfNecessary();
  glass_frame_view_->UpdateWindowTitle();

  WebAppFrameToolbarView* web_app_frame_toolbar =
      browser_view_->web_app_frame_toolbar_for_testing();

  // Verify that the center container doesn't consume space by expecting the
  // right container to consume the full width of the WebAppFrameToolbarView.
  EXPECT_EQ(web_app_frame_toolbar->width(),
            web_app_frame_toolbar->get_right_container_for_testing()->width());
}
