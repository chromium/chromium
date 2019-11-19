// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_POSIX)
#include <signal.h>
#endif

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using base::TimeDelta;
using content::BrowserThread;

const char NOLISTENERS_HTML[] =
    "<html><head><title>nolisteners</title></head><body></body></html>";

const char UNLOAD_HTML[] =
    "<html><head><title>unload</title></head><body>"
    "<script>window.onunload=function(e){}</script></body></html>";

const char BEFORE_UNLOAD_HTML[] =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
    "setTimeout('document.title=\"cancelled\"', 0);return 'foo'}</script>"
    "</body></html>";

const char INNER_FRAME_WITH_FOCUS_HTML[] =
    "<html><head><title>innerframewithfocus</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "<iframe src=\"data:text/html,<html><head><script>window.onload="
    "function(){document.getElementById('box').focus()}</script>"
    "<body><input id='box'></input></body></html>\"></iframe>"
    "</body></html>";

const char INFINITE_UNLOAD_HTML[] =
    "<html><head><title>infiniteunload</title></head><body>"
    "<script>window.onunload=function(e){while(true){}}</script>"
    "</body></html>";

const char INFINITE_BEFORE_UNLOAD_HTML[] =
    "<html><head><title>infinitebeforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){while(true){}}</script>"
    "</body></html>";

const char INFINITE_UNLOAD_ALERT_HTML[] =
    "<html><head><title>infiniteunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
    "while(true){}"
    "alert('foo');"
    "}</script></body></html>";

const char INFINITE_BEFORE_UNLOAD_ALERT_HTML[] =
    "<html><head><title>infinitebeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
    "while(true){}"
    "alert('foo');"
    "}</script></body></html>";

const char TWO_SECOND_UNLOAD_ALERT_HTML[] =
    "<html><head><title>twosecondunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
    "var start = new Date().getTime();"
    "while(new Date().getTime() - start < 2000){}"
    "alert('foo');"
    "}</script></body></html>";

const char TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML[] =
    "<html><head><title>twosecondbeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
    "var start = new Date().getTime();"
    "while(new Date().getTime() - start < 2000){}"
    "alert('foo');"
    "}</script></body></html>";

const char CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER[] =
    "<html><head><title>only_one_unload</title></head>"
    "<script>"
    "function openPopup() {"
    "  var w = window.open('about:blank');"
    "  w.document.write('<html><head><title>popup</title></head></body>');"
    "}"
    "</script>"
    "<body onclick='openPopup()' onbeforeunload='return;'>"
    "</body></html>";

class UnloadResults {
 public:
  UnloadResults() : successes_(0), aborts_(0) {}

  void AddSuccess(const base::FilePath&) { successes_++; }
  void AddAbort(const base::FilePath&) { aborts_++; }
  void AddError(const base::FilePath&) {
    ADD_FAILURE() << "AddError should not be called.";
  }

  int get_successes() { return successes_; }
  int get_aborts() { return aborts_; }

 private:
  int successes_;
  int aborts_;
};

class UnloadTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strstr(test_info->name(), "BrowserCloseTabWhenOtherTabHasListener") !=
        nullptr) {
      command_line->AppendSwitch(switches::kDisablePopupBlocking);
    } else if (strstr(test_info->name(), "BrowserTerminateBeforeUnload") !=
               nullptr) {
#if defined(OS_POSIX)
      DisableSIGTERMHandling();
#endif
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void CheckTitle(const char* expected_title, bool wait = false) {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    base::string16 expected = base::ASCIIToUTF16(expected_title);
    base::string16 actual;
    if (wait)
      actual = content::TitleWatcher(web_contents, expected).WaitAndGetTitle();
    else
      actual = web_contents->GetTitle();
    EXPECT_EQ(expected, actual);
  }

  void NavigateToDataURL(const char* html_content, const char* expected_title) {
    ui_test_utils::NavigateToURL(
        browser(), GURL(std::string("data:text/html,") + html_content));
    CheckTitle(expected_title);
  }

  void NavigateToNolistenersFileTwice() {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    ui_test_utils::NavigateToURL(browser(), url);
    CheckTitle("Title Of Awesomeness");
    ui_test_utils::NavigateToURL(browser(), url);
    CheckTitle("Title Of Awesomeness");
  }

  // Navigates to a URL asynchronously, then again synchronously. The first
  // load is purposely async to test the case where the user loads another
  // page without waiting for the first load to complete.
  void NavigateToNolistenersFileTwiceAsync() {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB, 0);
    ui_test_utils::NavigateToURL(browser(), url);
    CheckTitle("Title Of Awesomeness");
  }

  void LoadUrlAndQuitBrowser(const char* html_content,
                             const char* expected_title) {
    NavigateToDataURL(html_content, expected_title);
    CloseBrowserSynchronously(browser());
  }

  // If |accept| is true, simulates user clicking OK, otherwise simulates
  // clicking Cancel.
  void ClickModalDialogButton(bool accept) {
    app_modal::JavaScriptAppModalDialog* dialog =
        ui_test_utils::WaitForAppModalDialog();
    if (accept)
      dialog->native_dialog()->AcceptAppModalDialog();
    else
      dialog->native_dialog()->CancelAppModalDialog();
  }

  void PrepareForDialog(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); i++) {
      content::PrepContentsForBeforeUnloadTest(
          browser->tab_strip_model()->GetWebContentsAt(i));
    }
  }

  void CloseBrowsersVerifyUnloadSuccess(bool force) {
    UnloadResults unload_results;
    BrowserList::CloseAllBrowsersWithProfile(
        browser()->profile(),
        base::Bind(&UnloadResults::AddSuccess,
                   base::Unretained(&unload_results)),
        base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
        force);
    ui_test_utils::WaitForBrowserToClose();
    EXPECT_EQ(1, unload_results.get_successes());
    EXPECT_EQ(0, unload_results.get_aborts());
  }

  // The test harness cannot close the window automatically, because it requires
  // confirmation. We close the window manually instead.
  void ManuallyCloseWindow() {
    chrome::CloseWindow(browser());
    ClickModalDialogButton(true);
    ui_test_utils::WaitForBrowserToClose();
  }
};

// Navigate to a page with an infinite unload handler.
// Then two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
//
// This test is flaky on the valgrind UI bots. http://crbug.com/39057
IN_PROC_BROWSER_TEST_F(UnloadTest, CrossSiteInfiniteUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, "infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
}

// Navigate to a page with an infinite unload handler.
// Then two sync crosssite requests to ensure
// we correctly nav to each one.
IN_PROC_BROWSER_TEST_F(UnloadTest, CrossSiteInfiniteUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, "infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
// This test is flaky on the valgrind UI bots. http://crbug.com/39057 and
// http://crbug.com/86469
IN_PROC_BROWSER_TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, "infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two sync crosssite requests to ensure
// we correctly nav to each one.
// Flaky on Win, Linux, and Mac; http://crbug.com/462671.
IN_PROC_BROWSER_TEST_F(UnloadTest, DISABLED_CrossSiteInfiniteBeforeUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, "infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
}

// Tests closing the browser on a page with no unload listeners registered.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseNoUnloadListeners) {
  LoadUrlAndQuitBrowser(NOLISTENERS_HTML, "nolisteners");
}

// Tests closing the browser on a page with an unload listener registered.
// Test marked as flaky in http://crbug.com/51698
IN_PROC_BROWSER_TEST_F(UnloadTest, DISABLED_BrowserCloseUnload) {
  LoadUrlAndQuitBrowser(UNLOAD_HTML, "unload");
}

// Tests closing the browser with a beforeunload handler and clicking
// OK in the beforeunload confirm dialog.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseBeforeUnloadOK) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());

  chrome::CloseWindow(browser());
  ClickModalDialogButton(true);
  ui_test_utils::WaitForBrowserToClose();
}

// Tests closing the browser with a beforeunload handler and clicking
// CANCEL in the beforeunload confirm dialog.
// If this test flakes, reopen http://crbug.com/123110
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseBeforeUnloadCancel) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());
  chrome::CloseWindow(browser());

  // We wait for the title to change after cancelling the closure of browser
  // window, to ensure that in-flight IPCs from the renderer reach the browser.
  // Otherwise the browser won't put up the beforeunload dialog because it's
  // waiting for an ack from the renderer.
  base::string16 expected_title = base::ASCIIToUTF16("cancelled");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ClickModalDialogButton(false);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  ManuallyCloseWindow();
}

// Tests closing the browser by BrowserList::CloseAllBrowsersWithProfile,
// on a page with no unload listeners registered.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListCloseNoUnloadListeners) {
  NavigateToDataURL(NOLISTENERS_HTML, "nolisteners");

  CloseBrowsersVerifyUnloadSuccess(false);
}

// Tests closing the browser by BrowserList::CloseAllBrowsersWithProfile, with a
// beforeunload handler and clicking Leave in the beforeunload confirm dialog.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListCloseBeforeUnloadOK) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());

  UnloadResults unload_results;
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      false);
  ClickModalDialogButton(true);
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(1, unload_results.get_successes());
  EXPECT_EQ(0, unload_results.get_aborts());
}

IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListForceCloseNoUnloadListeners) {
  NavigateToDataURL(NOLISTENERS_HTML, "nolisteners");

  CloseBrowsersVerifyUnloadSuccess(true);
}

IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListForceCloseWithBeforeUnload) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");

  CloseBrowsersVerifyUnloadSuccess(true);
}

// Tests closing the browser by BrowserList::CloseAllBrowsersWithProfile, with a
// beforeunload handler and clicking Stay in the beforeunload confirm dialog.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListCloseBeforeUnloadCancel) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());

  UnloadResults unload_results;
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      false);

  // We wait for the title to change after cancelling the closure of browser
  // window, to ensure that in-flight IPCs from the renderer reach the browser.
  // Otherwise the browser won't put up the beforeunload dialog because it's
  // waiting for an ack from the renderer.
  base::string16 expected_title = base::ASCIIToUTF16("cancelled");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ClickModalDialogButton(false);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  EXPECT_EQ(0, unload_results.get_successes());
  EXPECT_EQ(1, unload_results.get_aborts());

  ManuallyCloseWindow();
}

// Tests double calls to BrowserList::CloseAllBrowsersWithProfile, with a
// beforeunload handler and clicking Leave in the beforeunload confirm dialog.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListDoubleCloseBeforeUnloadOK) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());

  UnloadResults unload_results;
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      false);
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      false);
  ClickModalDialogButton(true);
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(1, unload_results.get_successes());
  EXPECT_EQ(0, unload_results.get_aborts());
}

// Tests double calls to BrowserList::CloseAllBrowsersWithProfile, with a
// beforeunload handler and clicking Stay in the beforeunload confirm dialog.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListDoubleCloseBeforeUnloadCancel) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());

  UnloadResults unload_results;
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      false);
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      false);

  // We wait for the title to change after cancelling the closure of browser
  // window, to ensure that in-flight IPCs from the renderer reach the browser.
  // Otherwise the browser won't put up the beforeunload dialog because it's
  // waiting for an ack from the renderer.
  base::string16 expected_title = base::ASCIIToUTF16("cancelled");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ClickModalDialogButton(false);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  EXPECT_EQ(0, unload_results.get_successes());
  EXPECT_EQ(1, unload_results.get_aborts());

  ManuallyCloseWindow();
}

// Tests closing the browser by BrowserList::CloseAllBrowsersWithProfile, with
// a null success callback, a beforeunload handler and clicking Leave in the
// beforeunload confirm dialog. The test succeed if no crash happens.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListCloseBeforeUnloadNullCallbackOk) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());

  UnloadResults unload_results;
  BrowserList::CloseAllBrowsersWithProfile(browser()->profile(),
                                           BrowserList::CloseCallback(),
                                           BrowserList::CloseCallback(), false);
  ClickModalDialogButton(true);
  ui_test_utils::WaitForBrowserToClose();
}

// Tests closing the browser by BrowserList::CloseAllBrowsersWithProfile, with
// a null failure callback, a beforeunload handler and clicking Stay in the
// beforeunload confirm dialog. The test succeed if no crash happens.
IN_PROC_BROWSER_TEST_F(UnloadTest,
                       BrowserListCloseBeforeUnloadNullCallbackCancel) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  PrepareForDialog(browser());

  UnloadResults unload_results;
  BrowserList::CloseAllBrowsersWithProfile(browser()->profile(),
                                           BrowserList::CloseCallback(),
                                           BrowserList::CloseCallback(), false);

  // We wait for the title to change after cancelling the closure of browser
  // window, to ensure that in-flight IPCs from the renderer reach the browser.
  // Otherwise the browser won't put up the beforeunload dialog because it's
  // waiting for an ack from the renderer.
  base::string16 expected_title = base::ASCIIToUTF16("cancelled");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ClickModalDialogButton(false);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  ManuallyCloseWindow();
}

// Tests terminating the browser with a beforeunload handler.
// Currently only ChromeOS shuts down gracefully.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserTerminateBeforeUnload) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  EXPECT_EQ(kill(base::GetCurrentProcessHandle(), SIGTERM), 0);
}
#endif

// Tests closing the browser and clicking OK in the beforeunload confirm dialog
// if an inner frame has the focus.
// If this flakes, use http://crbug.com/32615 and http://crbug.com/45675
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseWithInnerFocusedFrame) {
  NavigateToDataURL(INNER_FRAME_WITH_FOCUS_HTML, "innerframewithfocus");
  PrepareForDialog(browser());

  ManuallyCloseWindow();
}

// Tests closing the browser with a beforeunload handler that takes forever
// by running an infinite loop.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseInfiniteBeforeUnload) {
  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_HTML,
                        "infinitebeforeunload");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseInfiniteUnload) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_HTML, "infiniteunload");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop followed by an alert.
// If this flakes, use http://crbug.com/86469
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseInfiniteUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_ALERT_HTML, "infiniteunloadalert");
}

// Tests closing the browser with a beforeunload handler that hangs then
// pops up an alert.
// If this flakes, use http://crbug.com/78803 and http://crbug.com/86469.
IN_PROC_BROWSER_TEST_F(UnloadTest,
                       DISABLED_BrowserCloseInfiniteBeforeUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_ALERT_HTML,
                        "infinitebeforeunloadalert");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an 2 second long loop followed by an alert.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseTwoSecondUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_UNLOAD_ALERT_HTML, "twosecondunloadalert");
}

// Tests closing the browser with a beforeunload handler that takes
// two seconds to run then pops up an alert.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseTwoSecondBeforeUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML,
                        "twosecondbeforeunloadalert");
}

// Tests that if there's a renderer process with two tabs, one of which has an
// unload handler, and the other doesn't, the tab that doesn't have an unload
// handler can be closed.
// If this flakes, see http://crbug.com/45162, http://crbug.com/45281 and
// http://crbug.com/86769.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseTabWhenOtherTabHasListener) {
  NavigateToDataURL(CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER, "only_one_unload");

  // Simulate a click to force user_gesture to true; if we don't, the resulting
  // popup will be constrained, which isn't what we want to test.

  ui_test_utils::TabAddedWaiter tab_add(browser());
  content::SimulateMouseClick(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft);
  tab_add.Wait();
  // Need to wait for the title, because the initial page (about:blank) can stop
  // loading before the click handler calls document.write.
  CheckTitle("popup", true);

  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::CloseTab(browser());
  destroyed_watcher.Wait();

  CheckTitle("only_one_unload");
}

IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserListForceCloseAfterNormalClose) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");

  UnloadResults unload_results;
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      false);
  BrowserList::CloseAllBrowsersWithProfile(
      browser()->profile(),
      base::Bind(&UnloadResults::AddSuccess, base::Unretained(&unload_results)),
      base::Bind(&UnloadResults::AddAbort, base::Unretained(&unload_results)),
      true);
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(1, unload_results.get_successes());
  EXPECT_EQ(0, unload_results.get_aborts());
}

// Tests that a cross-site iframe runs its beforeunload handler when closing
// the browser.  See https://crbug.com/853021.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseWithCrossSiteIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate iframe cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", frame_url));

  // Install a dialog-showing beforeunload handler in the iframe.
  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  EXPECT_TRUE(
      ExecuteScript(child, "window.onbeforeunload = () => { return 'x' };"));

  // Close the browser and make sure the beforeunload dialog is shown and can
  // be clicked.
  PrepareForDialog(browser());
  ManuallyCloseWindow();
}

// Tests that a same-site iframe runs its beforeunload handler when closing the
// browser.  See https://crbug.com/1010456.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseWithSameSiteIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with a same-site iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  EXPECT_EQ(child->GetSiteInstance(),
            web_contents->GetMainFrame()->GetSiteInstance());

  // Install a dialog-showing beforeunload handler in the iframe.
  EXPECT_TRUE(
      ExecuteScript(child, "window.onbeforeunload = () => { return 'x' };"));

  // Close the browser and make sure the beforeunload dialog is shown and can
  // be clicked.
  PrepareForDialog(browser());
  ManuallyCloseWindow();
}

// TODO(ojan): Add tests for unload/beforeunload that have multiple tabs
// and multiple windows.
