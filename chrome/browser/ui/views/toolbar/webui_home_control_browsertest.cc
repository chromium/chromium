// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/webui_home_control_test_base.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

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

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropPlainText_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  SimulateTextDrop("hello world", DragOrigin::kWebPage);

  ExpectSearchedFor("hello world");
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropPlainText_FromOs) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  SimulateTextDrop("hello world", DragOrigin::kOs);

  ExpectSearchedFor("hello world");
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropUrlText_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  const GURL url("https://www.example.test/");
  SimulateTextDrop(url.spec(), DragOrigin::kWebPage);

  ExpectNavigatedTo(url);
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropUrlLink_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  const GURL url("https://www.example.test/");
  SimulateLinkDrop(url.spec(), DragOrigin::kWebPage);

  ExpectHomePageSetTo(url);
}

// Dragging a real link produces both `text/uri-list` and `text/plain` holding
// the same URL. The link must win, otherwise this would navigate instead of
// setting the home page.
IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropUrlLinkWithTextFallback_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  const GURL link("https://www.example.test/");
  SimulateLinkWithTextDrop(link.spec(), DragOrigin::kWebPage);

  ExpectHomePageSetTo(link);
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropUrlText_FromOs) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  const GURL url("https://www.example.test/");
  SimulateTextDrop(url.spec(), DragOrigin::kOs);

  ExpectNavigatedTo(url);
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropFile_FromOs) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.html");
  ASSERT_TRUE(base::WriteFile(file_path, "<html><body>test</body></html>"));
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  SimulateFileDrop(file_path, DragOrigin::kOs);

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS cannot tell an OS-local drag from a renderer drag, so it treats
  // every drag as renderer-originated (b/256022714). The dragged file path is
  // then never cached, leaving the drop with nothing to act on.
  ExpectDropIgnored();
#else
  ExpectHomePageSetTo(net::FilePathToFileURL(file_path));
#endif
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropFilePath_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  SimulateTextDrop("file:///tmp/secret.html", DragOrigin::kWebPage);

  ExpectDropIgnored();
}

// A web page cannot point the browser at a local file through `text/plain`
// (see DropFilePath_FromWebPage), but it can through a link. This asymmetry
// replicates the native Views behavior and is deliberate.
IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropFileLink_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  const GURL url("file:///tmp/secret.html");
  SimulateLinkDrop(url.spec(), DragOrigin::kWebPage);

  ExpectHomePageSetTo(url);
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropFilePath_FromOs) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.html");
  ASSERT_TRUE(base::WriteFile(file_path, "<html><body>test</body></html>"));
  const GURL file_url = net::FilePathToFileURL(file_path);
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  SimulateTextDrop(file_url.spec(), DragOrigin::kOs);

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS cannot tell an OS-local drag from a renderer drag, so it treats
  // every drag as renderer-originated (b/256022714), which limits text drops
  // to HTTP/HTTPS.
  ExpectDropIgnored();
#else
  ExpectNavigatedTo(file_url);
#endif
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropJavaScriptText_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  ASSERT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
  SimulateTextDrop("javascript:void(document.title='PWNED')",
                   DragOrigin::kWebPage);

  ExpectDropIgnored();
  EXPECT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
}

// A nested pseudo-scheme must not survive a second round of unwrapping.
IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropNestedJavaScriptText_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  ASSERT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
  SimulateTextDrop("javascript:javascript:void(document.title='PWNED')",
                   DragOrigin::kWebPage);

  ExpectDropIgnored();
  EXPECT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropJavaScriptLink_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  ASSERT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
  SimulateLinkDrop("javascript:void(document.title='PWNED')",
                   DragOrigin::kWebPage);

  ExpectDropIgnored();
  EXPECT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropNestedJavaScriptLink_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  ASSERT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
  SimulateLinkDrop("javascript:javascript:void(document.title='PWNED')",
                   DragOrigin::kWebPage);

  ExpectDropIgnored();
  EXPECT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropJavaScriptText_FromOs) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  ASSERT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
  SimulateTextDrop("javascript:void(document.title='PWNED')", DragOrigin::kOs);

  ExpectDropIgnored();
  EXPECT_EQ("", content::EvalJs(active_web_contents(), "document.title"));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropPrivilegedUrlText_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  SimulateTextDrop("chrome://settings", DragOrigin::kWebPage);

  ExpectDropIgnored();
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropPrivilegedUrlLink_FromWebPage) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  SimulateLinkDrop("chrome://settings", DragOrigin::kWebPage);

  ExpectDropIgnored();
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest,
                       DropPrivilegedUrlText_FromOs) {
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());
  const GURL url("chrome://version/");
  SimulateTextDrop(url.spec(), DragOrigin::kOs);

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS cannot tell an OS-local drag from a renderer drag, so it treats
  // every drag as renderer-originated (b/256022714), which limits text drops
  // to HTTP/HTTPS.
  ExpectDropIgnored();
#else
  ExpectNavigatedTo(url);
#endif
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropUrlLinkAndUndo) {
  PrefService* prefs = browser()->GetProfile()->GetPrefs();

  prefs->SetString(prefs::kHomePage, "https://www.url-a.test");
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());

  const GURL url("https://www.url-b.test/");
  SimulateLinkDrop(url.spec(), DragOrigin::kWebPage);
  ExpectHomePageSetTo(url);

  PerformUndo();

  EXPECT_EQ("https://www.url-a.test/", prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropUrlLinkAndUndoFromNtp) {
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());

  const GURL url("https://www.example.test/");
  SimulateLinkDrop(url.spec(), DragOrigin::kWebPage);
  ExpectHomePageSetTo(url);

  PerformUndo();

  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(WebUIHomeControlBrowserTest, DropFileAndUndo) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.html");
  ASSERT_TRUE(base::WriteFile(file_path, "<html><body>test</body></html>"));

  ASSERT_NO_FATAL_FAILURE(SetUpHomeButtonDropTest());

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS cannot tell an OS-local drag from a renderer drag, so it treats
  // every drag as renderer-originated (b/256022714). The dragged file path is
  // then never cached, so the home page is never set and there is nothing to
  // undo.
  SimulateFileDrop(file_path, DragOrigin::kOs);
  ExpectDropIgnored();
#else
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  const GURL old_url(prefs->GetString(prefs::kHomePage));
  const bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  SimulateFileDrop(file_path, DragOrigin::kOs);
  ExpectHomePageSetTo(net::FilePathToFileURL(file_path));

  PerformUndo();

  EXPECT_EQ(old_url.spec(), prefs->GetString(prefs::kHomePage));
  EXPECT_EQ(old_is_ntp, prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
#endif
}
