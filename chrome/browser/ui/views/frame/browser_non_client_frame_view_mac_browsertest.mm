// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

namespace {

class TextChangeWaiter {
 public:
  explicit TextChangeWaiter(views::Label* label)
      : subscription_(label->AddTextChangedCallback(
            base::BindRepeating(&TextChangeWaiter::OnTextChanged,
                                base::Unretained(this)))) {}

  TextChangeWaiter(const TextChangeWaiter&) = delete;
  TextChangeWaiter& operator=(const TextChangeWaiter&) = delete;

  // Runs a loop until a text change is observed (unless one has
  // already been observed, in which case it returns immediately).
  void Wait() {
    if (observed_change_)
      return;

    run_loop_.Run();
  }

 private:
  void OnTextChanged() {
    observed_change_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  bool observed_change_ = false;
  base::RunLoop run_loop_;
  base::CallbackListSubscription subscription_;
};

}  // anonymous namespace

enum class PrefixTitles { kEnabled, kDisabled };

class BrowserNonClientFrameViewMacBrowserTestTitlePrefixed
    : public web_app::WebAppControllerBrowserTest,
      public testing::WithParamInterface<PrefixTitles> {
 public:
  BrowserNonClientFrameViewMacBrowserTestTitlePrefixed() {
    if (GetParam() == PrefixTitles::kEnabled) {
      features_.InitAndEnableFeature(features::kPrefixWebAppWindowsWithAppName);
    } else {
      features_.InitAndDisableFeature(
          features::kPrefixWebAppWindowsWithAppName);
    }
  }
  ~BrowserNonClientFrameViewMacBrowserTestTitlePrefixed() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewMacBrowserTestTitlePrefixed,
                       TitleUpdates) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL start_url = GetInstallableAppURL();
  const web_app::AppId app_id = InstallPWA(start_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Ensure the main page has loaded and is ready for ExecJs DOM manipulation.
  ASSERT_TRUE(content::NavigateToURL(web_contents, start_url));

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::NonClientFrameView* const frame_view =
      browser_view->GetWidget()->non_client_view()->frame_view();
  auto* const title =
      static_cast<views::Label*>(frame_view->GetViewByID(VIEW_ID_WINDOW_TITLE));

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    std::u16string expected_title(u"Full Screen");
    if (GetParam() == PrefixTitles::kEnabled)
      expected_title = base::StrCat({u"A Web App - ", expected_title});
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(expected_title, title->GetText());
  }

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    std::u16string expected_title(u"Not Full Screen");
    if (GetParam() == PrefixTitles::kEnabled)
      expected_title = base::StrCat({u"A Web App - ", expected_title});
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Not Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(expected_title, title->GetText());
  }
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         BrowserNonClientFrameViewMacBrowserTestTitlePrefixed,
                         testing::Values(PrefixTitles::kEnabled,
                                         PrefixTitles::kDisabled));

using BrowserNonClientFrameViewMacBrowserTest =
    web_app::WebAppControllerBrowserTest;

// Test to make sure the WebAppToolbarFrame triggers an InvalidateLayout() when
// toggled in fullscreen mode.
// TODO(crbug.com/1156050): Flaky on Mac.
#if defined(OS_MAC)
#define MAYBE_ToolbarLayoutFullscreenTransition \
  DISABLED_ToolbarLayoutFullscreenTransition
#else
#define MAYBE_ToolbarLayoutFullscreenTransition \
  ToolbarLayoutFullscreenTransition
#endif
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewMacBrowserTest,
                       MAYBE_ToolbarLayoutFullscreenTransition) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL start_url = GetInstallableAppURL();
  const web_app::AppId app_id = InstallPWA(start_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  BrowserNonClientFrameView* const frame_view =
      static_cast<BrowserNonClientFrameView*>(
          browser_view->GetWidget()->non_client_view()->frame_view());

  // Trigger a layout on the view tree to address any invalid layouts waiting
  // for a re-layout.
  views::ViewTestApi frame_view_test_api(frame_view);
  browser_view->GetWidget()->LayoutRootViewIfNecessary();

  // Assert that the layout of the frame view is in a valid state.
  EXPECT_FALSE(frame_view_test_api.needs_layout());

  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShowFullscreenToolbar, false);

  chrome::ToggleFullscreenMode(browser);
  fake_fullscreen.FinishTransition();
  EXPECT_FALSE(frame_view_test_api.needs_layout());

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);

  // Showing the toolbar in fullscreen mode should trigger a layout
  // invalidation.
  EXPECT_TRUE(frame_view_test_api.needs_layout());
}

class WebAppBrowserFrameViewMacWindowControlsOverlayTest
    : public InProcessBrowserTest {
 public:
  WebAppBrowserFrameViewMacWindowControlsOverlayTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        features::kWebAppWindowControlsOverlay);
  }
  WebAppBrowserFrameViewMacWindowControlsOverlayTest(
      const WebAppBrowserFrameViewMacWindowControlsOverlayTest&) = delete;
  WebAppBrowserFrameViewMacWindowControlsOverlayTest& operator=(
      const WebAppBrowserFrameViewMacWindowControlsOverlayTest&) = delete;

  ~WebAppBrowserFrameViewMacWindowControlsOverlayTest() override = default;

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

  void InstallAndLaunchWebAppWithWindowControlsOverlay() {
    GURL start_url = LoadTestPageWithDataAndGetURL();

    std::vector<blink::mojom::DisplayMode> display_overrides;
    display_overrides.emplace_back(
        blink::mojom::DisplayMode::kWindowControlsOverlay);
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
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

    frame_view_ = static_cast<BrowserNonClientFrameViewMac*>(frame_view);
    auto* web_app_frame_toolbar =
        frame_view_->web_app_frame_toolbar_for_testing();

    DCHECK(web_app_frame_toolbar);
    DCHECK(web_app_frame_toolbar->GetVisible());
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
  BrowserNonClientFrameViewMac* frame_view_ = nullptr;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       WindowControlsOverlay) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

  // Toggle overlay on, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayEnabledAndWait();

  gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_NE(0, bounds.x());
  EXPECT_EQ(0, bounds.y());
  EXPECT_FALSE(bounds.IsEmpty());

  // Toggle overlay off, and validate JS API reflects the expected
  // values.
  ToggleWindowControlsOverlayEnabledAndWait();
  bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_FALSE(GetWindowControlOverlayVisibility());
  EXPECT_EQ(gfx::Rect(), bounds);
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       GeometryChangeEvent) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();
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

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       NoGeometryChangeEventIfOverlayIsOff) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

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

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       WindowControlsOverlayDraggableRegions) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();
  ToggleWindowControlsOverlayEnabledAndWait();

  constexpr gfx::Point kPoint(50, 50);
  EXPECT_EQ(frame_view_->NonClientHitTest(kPoint), HTCAPTION);
  EXPECT_FALSE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetWidget()->GetNativeView(), kPoint));

  // Validate that a point at the border that does not intersect with the
  // overlays is not marked as draggable.
  constexpr gfx::Point kBorderPoint(100, 1);
  EXPECT_NE(frame_view_->NonClientHitTest(kBorderPoint), HTCAPTION);
  EXPECT_TRUE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetWidget()->GetNativeView(), kBorderPoint));

  // Validate that the draggable region is reset on navigation and the point is
  // no longer draggable.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_view_->browser(),
                                           GURL("http://example.test/")));
  EXPECT_NE(frame_view_->NonClientHitTest(kPoint), HTCAPTION);
  EXPECT_TRUE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetWidget()->GetNativeView(), kPoint));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       WindowControlsOverlayRTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());
  InstallAndLaunchWebAppWithWindowControlsOverlay();
  ToggleWindowControlsOverlayEnabledAndWait();

  const gfx::Rect bounds = GetWindowControlOverlayBoundingClientRect();
  EXPECT_TRUE(GetWindowControlOverlayVisibility());
  EXPECT_NE(0, bounds.x());
  EXPECT_EQ(0, bounds.y());
  EXPECT_FALSE(bounds.IsEmpty());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       CSSRectTestLTR) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();
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

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       CSSRectTestRTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());
  InstallAndLaunchWebAppWithWindowControlsOverlay();
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

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       ToggleWindowControlsOverlay) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

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

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       OpenInChrome) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

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
