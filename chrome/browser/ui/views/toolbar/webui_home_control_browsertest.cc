// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/webui_home_control_test_base.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"

// Tests for the home button. Also serve as the general PressHandler tests.
class WebUIHomeControlBrowserTest : public WebUIHomeControlTestBase {};

// Home icon is different for touch only with old icon set.
class WebUIHomeControlOldIconsBrowserTest : public WebUIHomeControlBrowserTest {
 public:
  WebUIHomeControlOldIconsBrowserTest() {
    feature_list_.InitAndDisableFeature(features::kRoundedIcons);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, ClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  const struct {
    const char* name;
    std::string script;
  } test_cases[] = {
      {"Mouse Click", DispatchPointerDownAndUp(kHomeSelector)},
      {"Keyboard Click",
       DispatchEventScript(kHomeSelector, "MouseEvent", "click", "detail: 0")}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    const std::string& script = test_case.script;
    // Navigate away so clicking home actually does something.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

    // Click the button.
    content::TestNavigationObserver nav_observer(
        browser()->GetTabStripModel()->GetActiveWebContents());
    EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), script));
    nav_observer.Wait();

    EXPECT_EQ(home_url, browser()
                            ->GetTabStripModel()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL());
  }
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, CtrlClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  int initial_tab_count = browser()->GetTabStripModel()->count();
  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());

#if BUILDFLAG(IS_MAC)
  const char* kModifier = "metaKey: true";
#else
  const char* kModifier = "ctrlKey: true";
#endif

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerDownAndUp(
          kHomeSelector, "mouse",
          base::StrCat({"detail: 1, button: 0, ", kModifier}))));

  tab_add_waiter.Wait();

  EXPECT_EQ(initial_tab_count + 1, browser()->GetTabStripModel()->count());
  // Verify new tab is in the background.
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  content::WebContents* new_tab =
      browser()->GetTabStripModel()->GetWebContentsAt(initial_tab_count);
  content::TestNavigationObserver observer(new_tab);
  if (new_tab->GetLastCommittedURL() != home_url) {
    observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(home_url, new_tab->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, CtrlShiftClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  int initial_tab_count = browser()->GetTabStripModel()->count();
  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());

#if BUILDFLAG(IS_MAC)
  const char* kModifier = "metaKey: true";
#else
  const char* kModifier = "ctrlKey: true";
#endif

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerDownAndUp(kHomeSelector, "mouse",
                               base::StrCat({"detail: 1, button: 0, ",
                                             kModifier, ", shiftKey: true"}))));

  tab_add_waiter.Wait();

  EXPECT_EQ(initial_tab_count + 1, browser()->GetTabStripModel()->count());
  // Verify new tab is in the foreground.
  EXPECT_EQ(initial_tab_count, browser()->GetTabStripModel()->active_index());

  content::WebContents* new_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_tab);
  if (new_tab->GetLastCommittedURL() != home_url) {
    observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(home_url, new_tab->GetLastCommittedURL());
}

// Test the case the mouse is released over the home button without pressing on
// it.
IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       ReleaseOnHomeButtonWithoutPress) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  // Navigate away so clicking home actually does something.
  GURL other_url("chrome://version");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_url));

  // Release the pointer over the button.
  NavigationCounter nav_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());
  std::string script = base::StringPrintf(
      R"((() => {
          const target = %s;
          %s
          %s
          // Up event with no matching down event.
          target.dispatchEvent(new PointerEvent('pointerup',
              {bubbles: true, cancelable: true, view: window,
                pointerType: 'mouse', clientX: x, clientY: y,
                detail: 1, button: 0}));
      })();)",
      GetButtonIconJS(kHomeSelector), kGetCoordinatesJS,
      AddMockPointerCaptureFunctions("target").c_str());
  EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), script));

  nav_observer.WaitForNoNavigations();

  EXPECT_EQ(other_url, browser()
                           ->GetTabStripModel()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlOldIconsBrowserTest,
                       TouchModeChangesIcon) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  std::string get_icon_js = base::StrCat(
      {GetButtonIconJS(kHomeSelector), ".getAttribute('iron-icon')"});
  std::string get_button_height_js =
      base::StrCat({GetButtonIconJS(kHomeSelector), ".offsetHeight"});

  // Verify standard mode icon
  EXPECT_EQ("webui-toolbar:navigate_home_chrome_refresh_old",
            content::EvalJs(web_contents, get_icon_js));

  // Also its size.
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kToolbarButtonHeight),
            content::EvalJs(web_contents, get_button_height_js));

  {
    ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(true);

    // Wait and verify Touch mode icon
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return GetLayoutConstant(LayoutConstant::kToolbarButtonHeight) ==
             content::EvalJs(web_contents, get_button_height_js);
    }));
    EXPECT_EQ("webui-toolbar:navigate_home_touch_old",
              content::EvalJs(web_contents, get_icon_js));
  }

  // Revert to non-touch mode happens automatically when scoper goes out of
  // scope

  // Wait and verify standard mode icon again
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetLayoutConstant(LayoutConstant::kToolbarButtonHeight) ==
           content::EvalJs(web_contents, get_button_height_js);
  }));
  EXPECT_EQ("webui-toolbar:navigate_home_chrome_refresh_old",
            content::EvalJs(web_contents, get_icon_js));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, ShiftClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  ui_test_utils::BrowserCreatedObserver new_browser_observer;

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerDownAndUp(kHomeSelector, "mouse",
                               "detail: 1, button: 0, shiftKey: true")));

  BrowserWindowInterface* new_browser = new_browser_observer.Wait();
  ASSERT_TRUE(new_browser);

  content::WebContents* new_tab =
      new_browser->GetTabStripModel()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_tab);
  if (new_tab->GetLastCommittedURL() != home_url) {
    observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(home_url, new_tab->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DragAndDropHomeButton) {
  std::string current_home_url =
      browser()->GetProfile()->GetPrefs()->GetString(prefs::kHomePage);
  std::string new_home_url = "https://www.example.test/";
  EXPECT_NE(current_home_url, new_home_url);

  PerformDragAndDrop(new_home_url);
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DragAndDropHomeButton_BlockedJavascript) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  std::string default_homepage = prefs->GetString(prefs::kHomePage);

  // Directly call the drop URL method with a javascript: URL.
  std::string malicious_url = "javascript:alert(1)";
  webui_toolbar_view->OnHomeButtonDropUrl(GURL(malicious_url));

  // Verify the homepage preference has NOT changed.
  EXPECT_EQ(default_homepage, prefs->GetString(prefs::kHomePage));

  // Also verify NO undo bubble appeared.
  EXPECT_EQ(
      nullptr,
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
          views::ElementTrackerViews::GetContextForView(webui_toolbar_view)));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DragAndDropHomeButtonAndUndo) {
  auto* const prefs = browser()->GetProfile()->GetPrefs();
  prefs->SetString(prefs::kHomePage, "https://www.url-a.test");
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  base::RunLoop().RunUntilIdle();

  WebUIToolbarWebView* webui_toolbar_view =
      PerformDragAndDrop("https://www.url-b.test/");
  PerformUndo(webui_toolbar_view);

  // Verify the home page is reverted.
  EXPECT_EQ("https://www.url-a.test/", prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DragAndDropHomeButtonAndUndoFromNTP) {
  auto* const prefs = browser()->GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  base::RunLoop().RunUntilIdle();

  WebUIToolbarWebView* webui_toolbar_view =
      PerformDragAndDrop("https://www.example.test/");
  PerformUndo(webui_toolbar_view);

  // Verify the home page is reverted.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

// On ChromeOS, drag origin provenance cannot be distinguished between OS-local
// and renderer sources (b/256022714), so all drags are conservatively treated
// as renderer-originated and home button file drop is blocked.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DropFileOnHomeButtonAndUndo DISABLED_DropFileOnHomeButtonAndUndo
#else
#define MAYBE_DropFileOnHomeButtonAndUndo DropFileOnHomeButtonAndUndo
#endif
IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       MAYBE_DropFileOnHomeButtonAndUndo) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  content::WebContents* web_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();

  std::string file_path = "/fake/path/to/file.pdf";

  // Get the coordinates of the home button and dispatch event via hit-testing.
  gfx::Point center = BrowserElements::From(browser())
                          ->GetElement(kToolbarHomeButtonElementId)
                          ->GetScreenBounds()
                          .CenterPoint();
  gfx::Point click_point =
      center - webui_toolbar_view->GetBoundsInScreen().OffsetFromOrigin();

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  GURL old_url = GURL(prefs->GetString(prefs::kHomePage));
  bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  content::DropData drop_data;
  drop_data.filenames.emplace_back(base::FilePath::FromUTF8Unsafe(file_path),
                                   base::FilePath());

  webui_toolbar_view->GetWebViewForTesting()
      ->GetWebContents()
      ->GetDelegate()
      ->PreHandleDragUpdate(drop_data, gfx::PointF(click_point));

  // Now actually dispatch the drop event.
  EXPECT_EQ("success",
            content::EvalJs(web_contents, base::StringPrintf(R"(
    (function() {
      const target = document.querySelector('toolbar-app').shadowRoot
                       .querySelector('#home').shadowRoot
                       .querySelector('cr-icon-button');
      const dataTransfer = new DataTransfer();
      Object.defineProperty(dataTransfer, 'types', {value: ['Files']});
      const dropEvent = new DragEvent('drop', {
        bubbles: true,
        cancelable: true,
        clientX: %d,
        clientY: %d,
        dataTransfer: dataTransfer
      });
      target.dispatchEvent(dropEvent);
      return 'success';
    })();
  )",
                                                             click_point.x(),
                                                             click_point.y())));

  // Wait for the undo bubble. This proves the Mojo call reached C++.
  WaitForUndoBubble(webui_toolbar_view);

  GURL expected_url =
      net::FilePathToFileURL(base::FilePath::FromUTF8Unsafe(file_path));
  EXPECT_EQ(prefs->GetString(prefs::kHomePage), expected_url.spec());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));

  PerformUndo(webui_toolbar_view);

  // Verify that the pref is restored.
  EXPECT_EQ(prefs->GetString(prefs::kHomePage), old_url.spec());
  EXPECT_EQ(prefs->GetBoolean(prefs::kHomePageIsNewTabPage), old_is_ntp);
}
