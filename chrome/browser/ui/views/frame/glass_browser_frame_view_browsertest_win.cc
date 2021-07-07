// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/views/view_utils.h"

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
    auto web_app_info = std::make_unique<WebApplicationInfo>();
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

    web_app_frame_toolbar_ =
        glass_frame_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());
    return true;
  }

  absl::optional<SkColor> theme_color_ = SK_ColorBLUE;
  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  GlassBrowserFrameView* glass_frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;
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

  EXPECT_EQ(glass_frame_view_->GetTitlebarColor(),
            ThemeProperties::GetDefaultColor(
                ThemeProperties::COLOR_FRAME_ACTIVE, false));
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, MaximizedLayout) {
  if (!InstallAndLaunchWebApp())
    return;

  glass_frame_view_->frame()->Maximize();
  static_cast<views::View*>(glass_frame_view_)->Layout();

  DCHECK_GT(glass_frame_view_->window_title_for_testing()->x(), 0);
  DCHECK_GT(glass_frame_view_->web_app_frame_toolbar_for_testing()->y(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewTest, RTLTopRightHitTest) {
  base::i18n::SetRTLForTesting(true);
  if (!InstallAndLaunchWebApp())
    return;

  static_cast<views::View*>(glass_frame_view_)->Layout();

  // Avoid the top right resize corner.
  constexpr int kInset = 10;
  EXPECT_EQ(glass_frame_view_->NonClientHitTest(
                gfx::Point(glass_frame_view_->width() - kInset, kInset)),
            HTCAPTION);
}

class WebAppGlassBrowserFrameViewWindowControlsOverlayTest
    : public InProcessBrowserTest {
 public:
  WebAppGlassBrowserFrameViewWindowControlsOverlayTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        features::kWebAppWindowControlsOverlay);
  }
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

  GURL LoadTestPageWithDataAndGetURL() {
    // Write |data| to a temporary file that can be later reached at
    // http://127.0.0.1/test_file_*.html.
    static int s_test_file_number = 1;

    constexpr char kTestHTML[] =
        "<!DOCTYPE html>"
        "<style>"
        "  #target {"
        "    -webkit-app-region: drag;"
        "     height: 100px;"
        "     width: 100px;"
        "     padding-left: env(titlebar-area-x);"
        "     padding-right: env(titlebar-area-width);"
        "     padding-top: env(titlebar-area-y);"
        "     padding-bottom: env(titlebar-area-height);"
        "  }"
        "</style>"
        "<div id=target></div>";

    base::FilePath file_path = temp_dir_.GetPath().AppendASCII(
        base::StringPrintf("test_file_%d.html", s_test_file_number++));
    base::ScopedAllowBlockingForTesting allow_temp_file_writing;
    base::WriteFile(file_path, kTestHTML);
    GURL url = embedded_test_server()->GetURL(
        "/" + file_path.BaseName().AsUTF8Unsafe());
    return url;
  }

  bool InstallAndLaunchWebAppWithWindowControlsOverlay() {
    GURL start_url = LoadTestPageWithDataAndGetURL();

    std::vector<blink::mojom::DisplayMode> display_overrides;
    display_overrides.emplace_back(
        blink::mojom::DisplayMode::kWindowControlsOverlay);
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->open_as_window = true;
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
        glass_frame_view_->web_app_frame_toolbar_for_testing();

    DCHECK(web_app_frame_toolbar);
    DCHECK(web_app_frame_toolbar->GetVisible());
    return true;
  }

  void RunCallbackAndWaitForGeometryChangeEvent(base::OnceClosure callback) {
    auto* web_contents = browser_view_->GetActiveWebContents();
    EXPECT_TRUE(
        ExecJs(web_contents->GetMainFrame(),
               "geometrychangeCount = 0;"
               "document.title = 'beforegeometrychange';"
               "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
               "  geometrychangeCount++;"
               "  overlay_rect_from_event = e.boundingRect;"
               "  overlay_visible_from_event = e.visible;"
               "  document.title = 'ongeometrychange';"
               "}"));

    std::move(callback).Run();
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    ignore_result(title_watcher.WaitAndGetTitle());
  }

  void ToggleWindowControlsOverlayEnabledAndWait() {
    RunCallbackAndWaitForGeometryChangeEvent(base::BindLambdaForTesting(
        [this]() { browser_view_->ToggleWindowControlsOverlayEnabled(); }));
  }

  void ResizeWindowBoundsAndWait(const gfx::Rect& new_bounds) {
    // Changing the width of widget should trigger a "geometrychange" event.
    EXPECT_NE(new_bounds.width(), browser_view_->GetLocalBounds().width());
    RunCallbackAndWaitForGeometryChangeEvent(base::BindLambdaForTesting(
        [&]() { browser_view_->GetWidget()->SetBounds(new_bounds); }));
  }

  gfx::Rect GetWindowControlOverlayBoundingClientRectFromEvent() {
    auto* web_contents = browser_view_->GetActiveWebContents();
    return gfx::Rect(
        EvalJs(web_contents, "overlay_rect_from_event.x").ExtractInt(),
        EvalJs(web_contents, "overlay_rect_from_event.y").ExtractInt(),
        EvalJs(web_contents, "overlay_rect_from_event.width").ExtractInt(),
        EvalJs(web_contents, "overlay_rect_from_event.height").ExtractInt());
  }

  gfx::Rect GetWindowControlOverlayBoundingClientRect() {
    auto* web_contents = browser_view_->GetActiveWebContents();
    return gfx::Rect(
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getBoundingClientRect().x")
            .ExtractInt(),
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getBoundingClientRect().y")
            .ExtractInt(),
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getBoundingClientRect().width")
            .ExtractInt(),
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getBoundingClientRect().height")
            .ExtractInt());
  }

  bool GetWindowControlOverlayVisibility() {
    auto* web_contents = browser_view_->GetActiveWebContents();
    return EvalJs(web_contents,
                  "window.navigator.windowControlsOverlay.visible")
        .ExtractBool();
  }

  bool GetWindowControlOverlayVisibilityFromEvent() {
    auto* web_contents = browser_view_->GetActiveWebContents();
    return EvalJs(web_contents, "overlay_visible_from_event").ExtractBool();
  }

  BrowserView* browser_view_ = nullptr;
  GlassBrowserFrameView* glass_frame_view_ = nullptr;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       WindowControlsOverlay) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  // Toggle overlay on, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayEnabledAndWait();

  gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_EQ(gfx::Point(), bounds.origin());
  EXPECT_FALSE(bounds.IsEmpty());

  // Toggle overlay off, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayEnabledAndWait();
  bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_FALSE(GetWindowControlOverlayVisibility());
  EXPECT_EQ(gfx::Rect(), bounds);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       GeometryChangeEvent) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();

  // Store the initial bounding client rect for comparison later.
  const gfx::Rect initial_js_overlay_bounds =
      GetWindowControlOverlayBoundingClientRect();
  gfx::Rect new_bounds = browser_view_->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() - 1);
  ResizeWindowBoundsAndWait(new_bounds);

  // Validate both the event payload and JS bounding client rect reflect
  // the new size.
  const gfx::Rect resized_js_overlay_bounds =
      GetWindowControlOverlayBoundingClientRect();
  const gfx::Rect resized_js_overlay_event_bounds =
      GetWindowControlOverlayBoundingClientRectFromEvent();
  EXPECT_EQ(
      1, EvalJs(browser_view_->GetActiveWebContents(), "geometrychangeCount"));
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_TRUE(GetWindowControlOverlayVisibilityFromEvent());
  EXPECT_EQ(resized_js_overlay_bounds, resized_js_overlay_event_bounds);
  EXPECT_EQ(initial_js_overlay_bounds.origin(),
            resized_js_overlay_bounds.origin());
  EXPECT_NE(initial_js_overlay_bounds.width(),
            resized_js_overlay_bounds.width());
  EXPECT_EQ(initial_js_overlay_bounds.height(),
            resized_js_overlay_bounds.height());
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       NoGeometryChangeEventIfOverlayIsOff) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  constexpr char kTestScript[] =
      "document.title = 'beforeevent';"
      "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
      "  document.title = 'ongeometrychange';"
      "};"
      "window.onresize = (e) => {"
      "  document.title = 'onresize';"
      "};";

  // Window Control Overlay is off by default.
  auto* web_contents = browser_view_->GetActiveWebContents();
  gfx::Rect new_bounds = browser_view_->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 10);
  EXPECT_TRUE(ExecJs(web_contents->GetMainFrame(), kTestScript));
  browser_view_->GetWidget()->SetBounds(new_bounds);
  content::TitleWatcher title_watcher(web_contents, u"onresize");
  title_watcher.AlsoWaitForTitle(u"ongeometrychange");
  EXPECT_EQ(u"onresize", title_watcher.WaitAndGetTitle());

  // Toggle Window Control Ovleray on and then off.
  ToggleWindowControlsOverlayEnabledAndWait();
  ToggleWindowControlsOverlayEnabledAndWait();

  // Validate event is not fired.
  new_bounds.set_width(new_bounds.width() - 10);
  EXPECT_TRUE(ExecJs(web_contents->GetMainFrame(), kTestScript));
  browser_view_->GetWidget()->SetBounds(new_bounds);
  content::TitleWatcher title_watcher2(web_contents, u"onresize");
  title_watcher2.AlsoWaitForTitle(u"ongeometrychange");
  EXPECT_EQ(u"onresize", title_watcher2.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       WindowControlsOverlayDraggableRegions) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();

  constexpr gfx::Point kPoint(50, 50);
  EXPECT_EQ(glass_frame_view_->NonClientHitTest(kPoint), HTCAPTION);
  EXPECT_FALSE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetNativeWindow(), kPoint));

  // Validate that a point at the border is not marked as draggable.
  constexpr gfx::Point kBorderPoint(50, 1);
  EXPECT_NE(glass_frame_view_->NonClientHitTest(kBorderPoint), HTCAPTION);
  EXPECT_TRUE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetWidget()->GetNativeView(), kBorderPoint));

  // Validate that the draggable region is reset on navigation and the point is
  // no longer draggable.
  ui_test_utils::NavigateToURL(browser_view_->browser(),
                               GURL("http://example.test/"));
  EXPECT_NE(glass_frame_view_->NonClientHitTest(kPoint), HTCAPTION);
  EXPECT_TRUE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetNativeWindow(), kPoint));
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       WindowControlsOverlayRTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();

  const gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_NE(0, bounds.x());
  EXPECT_EQ(0, bounds.y());
  EXPECT_FALSE(bounds.IsEmpty());
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       CSSRectTestLTR) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();

  constexpr char kTestScript[] =
      "var element = document.getElementById('target');"
      "var titlebarAreaX = "
      "    getComputedStyle(element).getPropertyValue('padding-left');"
      "var titlebarAreaXInt = parseInt(titlebarAreaX.split('px')[0]);"
      "var titlebarAreaY = "
      "    getComputedStyle(element).getPropertyValue('padding-top');"
      "var titlebarAreaYInt = parseInt(titlebarAreaY.split('px')[0]);"
      "var titlebarAreaWidthRect = "
      "    getComputedStyle(element).getPropertyValue('padding-right');"
      "var titlebarAreaWidthRectInt = "
      "    parseInt(titlebarAreaWidthRect.split('px')[0]);"
      "var titlebarAreaHeightRect = "
      "    getComputedStyle(element).getPropertyValue('padding-bottom');"
      "var titlebarAreaHeightRectInt = "
      "    parseInt(titlebarAreaHeightRect.split('px')[0]);";

  auto* web_contents = browser_view_->GetActiveWebContents();
  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kTestScript));

  const int initial_x_value =
      EvalJs(web_contents, "titlebarAreaXInt").ExtractInt();
  const int initial_y_value =
      EvalJs(web_contents, "titlebarAreaYInt").ExtractInt();
  const int initial_width_value =
      EvalJs(web_contents, "titlebarAreaWidthRectInt").ExtractInt();
  const int initial_height_value =
      EvalJs(web_contents, "titlebarAreaHeightRectInt").ExtractInt();

  EXPECT_EQ(0, initial_x_value);
  EXPECT_EQ(0, initial_y_value);
  EXPECT_NE(0, initial_width_value);
  EXPECT_NE(0, initial_height_value);

  // Change bounds so new values get sent.
  gfx::Rect new_bounds = browser_view_->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 20);
  new_bounds.set_height(new_bounds.height() + 20);
  ResizeWindowBoundsAndWait(new_bounds);

  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kTestScript));

  const int updated_x_value =
      EvalJs(web_contents, "titlebarAreaXInt").ExtractInt();
  const int updated_y_value =
      EvalJs(web_contents, "titlebarAreaYInt").ExtractInt();
  const int updated_width_value =
      EvalJs(web_contents, "titlebarAreaWidthRectInt").ExtractInt();
  const int updated_height_value =
      EvalJs(web_contents, "titlebarAreaHeightRectInt").ExtractInt();

  // Changing the window dimensions should only change the overlay width. The
  // overlay height should remain the same.
  EXPECT_EQ(initial_x_value, updated_x_value);
  EXPECT_EQ(initial_y_value, updated_y_value);
  EXPECT_NE(initial_width_value, updated_width_value);
  EXPECT_EQ(initial_height_value, updated_height_value);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       CSSRectTestRTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  ToggleWindowControlsOverlayEnabledAndWait();

  constexpr char kTestScript[] =
      "var element = document.getElementById('target');"
      "var titlebarAreaX = "
      "    getComputedStyle(element).getPropertyValue('padding-left');"
      "var titlebarAreaXInt = parseInt(titlebarAreaX.split('px')[0]);"
      "var titlebarAreaY = "
      "    getComputedStyle(element).getPropertyValue('padding-top');"
      "var titlebarAreaYInt = parseInt(titlebarAreaY.split('px')[0]);"
      "var titlebarAreaWidthRect = "
      "    getComputedStyle(element).getPropertyValue('padding-right');"
      "var titlebarAreaWidthRectInt = "
      "    parseInt(titlebarAreaWidthRect.split('px')[0]);"
      "var titlebarAreaHeightRect = "
      "    getComputedStyle(element).getPropertyValue('padding-bottom');"
      "var titlebarAreaHeightRectInt = "
      "    parseInt(titlebarAreaHeightRect.split('px')[0]);";

  auto* web_contents = browser_view_->GetActiveWebContents();
  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kTestScript));

  const int initial_x_value =
      EvalJs(web_contents, "titlebarAreaXInt").ExtractInt();
  const int initial_y_value =
      EvalJs(web_contents, "titlebarAreaYInt").ExtractInt();
  const int initial_width_value =
      EvalJs(web_contents, "titlebarAreaWidthRectInt").ExtractInt();
  const int initial_height_value =
      EvalJs(web_contents, "titlebarAreaHeightRectInt").ExtractInt();

  EXPECT_NE(0, initial_x_value);
  EXPECT_EQ(0, initial_y_value);
  EXPECT_NE(0, initial_width_value);
  EXPECT_NE(0, initial_height_value);

  // Change bounds so new values get sent.
  gfx::Rect new_bounds = browser_view_->GetLocalBounds();
  new_bounds.set_width(new_bounds.width() + 15);
  new_bounds.set_height(new_bounds.height() + 15);
  ResizeWindowBoundsAndWait(new_bounds);

  EXPECT_TRUE(ExecuteScript(web_contents->GetMainFrame(), kTestScript));

  const int updated_x_value =
      EvalJs(web_contents, "titlebarAreaXInt").ExtractInt();
  const int updated_y_value =
      EvalJs(web_contents, "titlebarAreaYInt").ExtractInt();
  const int updated_width_value =
      EvalJs(web_contents, "titlebarAreaWidthRectInt").ExtractInt();
  const int updated_height_value =
      EvalJs(web_contents, "titlebarAreaHeightRectInt").ExtractInt();

  // Changing the window dimensions should only change the overlay width. The
  // overlay height should remain the same.
  EXPECT_EQ(initial_x_value, updated_x_value);
  EXPECT_EQ(initial_y_value, updated_y_value);
  EXPECT_NE(initial_width_value, updated_width_value);
  EXPECT_EQ(initial_height_value, updated_height_value);
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       ToggleWindowControlsOverlay) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  // Make sure the app launches in standalone mode by default.
  EXPECT_FALSE(browser_view_->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(browser_view_->AppUsesWindowControlsOverlay());

  // Toggle WCO on, and verify that the UI updates accordingly.
  browser_view_->ToggleWindowControlsOverlayEnabled();
  EXPECT_TRUE(browser_view_->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(browser_view_->AppUsesWindowControlsOverlay());

  // Toggle WCO off, and verify that the app returns to 'standalone' mode.
  browser_view_->ToggleWindowControlsOverlayEnabled();
  EXPECT_FALSE(browser_view_->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(browser_view_->AppUsesWindowControlsOverlay());
}

IN_PROC_BROWSER_TEST_F(WebAppGlassBrowserFrameViewWindowControlsOverlayTest,
                       OpenInChrome) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    return;

  // Toggle overlay on, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayEnabledAndWait();

  // Validate non-empty bounds are being sent.
  EXPECT_TRUE(GetWindowControlOverlayVisibility());

  chrome::ExecuteCommand(browser_view_->browser(), IDC_OPEN_IN_CHROME);

  // Validate bounds are cleared.
  EXPECT_EQ(false, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                          "window.navigator.windowControlsOverlay.visible"));
}
