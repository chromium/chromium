// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
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
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

class BrowserNonClientFrameViewMacBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  BrowserNonClientFrameViewMacBrowserTest() = default;
  BrowserNonClientFrameViewMacBrowserTest(
      const BrowserNonClientFrameViewMacBrowserTest&) = delete;
  BrowserNonClientFrameViewMacBrowserTest& operator=(
      const BrowserNonClientFrameViewMacBrowserTest&) = delete;
  ~BrowserNonClientFrameViewMacBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewMacBrowserTest, TitleUpdates) {
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
    const std::u16string expected_title(u"Full Screen");
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
    const std::u16string expected_title(u"Not Full Screen");
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Not Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(expected_title, title->GetText());
  }
}

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

    const char kTestHTML[] = "<!DOCTYPE html>"
                             "<style>"
                             "  #target {"
                             "    -webkit-app-region: drag;"
                             "     height: 100px;"
                             "     width: 100px;"
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
    web_app_info->open_as_window = true;
    web_app_info->title = u"A Web App";
    web_app_info->display_override = display_overrides;

    web_app::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));

    content::TestNavigationObserver navigation_observer(start_url);
    base::RunLoop loop;
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

    // TODO(crbug.com/1191186): Register binder for BrowserInterfaceBroker
    // during testing.
    app_browser_->app_controller()->SetOnUpdateDraggableRegionForTesting(
        loop.QuitClosure());
    web_app::NavigateToURLAndWait(app_browser_, start_url);
    loop.Run();
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    frame_view_ = static_cast<BrowserNonClientFrameViewMac*>(frame_view);
    web_app_frame_toolbar_ = frame_view_->web_app_frame_toolbar_for_testing();

    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());
  }

  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  BrowserNonClientFrameViewMac* frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       WindowControlsOverlay) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

  browser_view_->ToggleWindowControlsOverlayEnabled();
  static_cast<views::View*>(frame_view_)->Layout();
  auto* web_contents = frame_view_->browser_view()->GetActiveWebContents();

  // window controls overlay should be not be an empty rect and visible as this
  // a web app.

  EXPECT_EQ(true, EvalJs(web_contents,
                         "window.navigator.windowControlsOverlay.visible"));

  EXPECT_NE(
      0, EvalJs(web_contents,
                "navigator.windowControlsOverlay.getBoundingClientRect().x"));
  EXPECT_EQ(
      0, EvalJs(web_contents,
                "navigator.windowControlsOverlay.getBoundingClientRect().y"));
  EXPECT_NE(
      0,
      EvalJs(web_contents,
             "navigator.windowControlsOverlay.getBoundingClientRect().width"));
  EXPECT_NE(
      0,
      EvalJs(web_contents,
             "navigator.windowControlsOverlay.getBoundingClientRect().height"));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       GeometryChangeEvent) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

  browser_view_->ToggleWindowControlsOverlayEnabled();
  auto* web_contents = frame_view_->browser_view()->GetActiveWebContents();

  EXPECT_TRUE(ExecuteScript(
      web_contents->GetMainFrame(),
      "geometrychangeCount = 0;"
      "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
      "  geometrychangeCount++;"
      "  rect = e.boundingRect;"
      "  visible = e.visible;"
      "}"));

  // Change size of widget to trigger a "geometrychange" event.
  gfx::Rect bounds = browser_view_->GetLocalBounds();
  bounds.set_width(bounds.width() - 1);
  browser_view_->GetWidget()->SetBounds(bounds);

  // Window controls overlay should be not be an empty rect and visible as this
  // is a web app.

  // expect the "geometrychange" event to have fired.
  EXPECT_NE(0, EvalJs(web_contents, "geometrychangeCount"));

  // Validate event payload.
  EXPECT_EQ(true, EvalJs(web_contents, "visible"));
  EXPECT_NE(0, EvalJs(web_contents, "rect.width"));
  EXPECT_NE(0, EvalJs(web_contents, "rect.height"));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       WindowControlsOverlayDraggableRegions) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

  browser_view_->ToggleWindowControlsOverlayEnabled();
  static_cast<views::View*>(frame_view_)->Layout();

  constexpr gfx::Point kPoint(50, 50);
  EXPECT_EQ(frame_view_->NonClientHitTest(kPoint), HTCAPTION);
  EXPECT_FALSE(browser_view_->ShouldDescendIntoChildForEventHandling(
      browser_view_->GetWidget()->GetNativeView(), kPoint));
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserFrameViewMacWindowControlsOverlayTest,
                       ToggleWindowControlsOverlay) {
  InstallAndLaunchWebAppWithWindowControlsOverlay();

  // Make sure it launches in standalone mode by default.
  EXPECT_FALSE(browser_view_->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(browser_view_->browser()
                  ->app_controller()
                  ->AppUsesWindowControlsOverlay());

  // Toggle WCO on, and verify that the UI updates accordingly.
  browser_view_->ToggleWindowControlsOverlayEnabled();
  EXPECT_TRUE(browser_view_->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(browser_view_->browser()
                  ->app_controller()
                  ->AppUsesWindowControlsOverlay());

  // Toggle WCO off, and verify that the app returns to 'standalone' mode
  browser_view_->ToggleWindowControlsOverlayEnabled();
  EXPECT_FALSE(browser_view_->IsWindowControlsOverlayEnabled());
  EXPECT_TRUE(browser_view_->browser()
                  ->app_controller()
                  ->AppUsesWindowControlsOverlay());
}
