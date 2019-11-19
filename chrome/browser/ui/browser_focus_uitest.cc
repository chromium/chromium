// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

using content::RenderViewHost;
using content::WebContents;

#if defined(OS_POSIX)
// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;
#endif

const char kSimplePage[] = "/focus/page_with_focus.html";
const char kStealFocusPage[] = "/focus/page_steals_focus.html";
const char kTypicalPage[] = "/focus/typical_page.html";

class BrowserFocusTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

  void ClickOnView(ViewID vid) { ui_test_utils::ClickOnView(browser(), vid); }

  void TestFocusTraversal(RenderViewHost* render_view_host, bool reverse) {
    const char kGetFocusedElementJS[] =
        "window.domAutomationController.send(getFocusedElement());";
    const char* kExpectedIDs[] = {"textEdit",   "searchButton", "luckyButton",
                                  "googleLink", "gmailLink",    "gmapLink"};
    SCOPED_TRACE(base::StringPrintf("TestFocusTraversal: reverse=%d", reverse));
    ui::KeyboardCode key = ui::VKEY_TAB;
#if defined(OS_MACOSX)
    // TODO(msw): Mac requires ui::VKEY_BACKTAB for reverse cycling. Sigh...
    key = reverse ? ui::VKEY_BACKTAB : ui::VKEY_TAB;
#endif

    // Loop through the focus chain twice for good measure.
    for (size_t i = 0; i < 2; ++i) {
      SCOPED_TRACE(base::StringPrintf("focus outer loop: %" PRIuS, i));
      ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

      // Mac requires an extra Tab key press to traverse the app menu button
      // iff "Full Keyboard Access" is enabled. In reverse, four Tab key presses
      // are required to traverse the back/forward buttons and the tab strip.
#if defined(OS_MACOSX)
      constexpr int kFocusableElementsBeforeOmnibox = 4;
      constexpr int kFocusableElementsAfterOmnibox = 1;
      if (ui_controls::IsFullKeyboardAccessEnabled()) {
        for (int j = 0; j < (reverse ? kFocusableElementsBeforeOmnibox
                                     : kFocusableElementsAfterOmnibox);
             ++j) {
          ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false,
                                                      reverse, false, false));
        }
      }
#endif

      if (reverse) {
        ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false,
                                                    reverse, false, false));
      }

      for (size_t j = 0; j < base::size(kExpectedIDs); ++j) {
        SCOPED_TRACE(base::StringPrintf("focus inner loop %" PRIuS, j));
        const size_t index = reverse ? base::size(kExpectedIDs) - 1 - j : j;
        // The details are the node's editable state, i.e. true for "textEdit".
        bool is_editable_node = index == 0;

        // Press Tab (or Shift+Tab) and check the focused element id.
        auto source = content::Source<RenderViewHost>(render_view_host);
        ui_test_utils::WindowedNotificationObserverWithDetails<bool> observer(
            content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE, source);
        ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false,
                                                    reverse, false, false));
        observer.Wait();
        bool observed_editable_node;
        ASSERT_TRUE(
            observer.GetDetailsFor(source.map_key(), &observed_editable_node));
        EXPECT_EQ(is_editable_node, observed_editable_node);

        std::string focused_id;
        EXPECT_TRUE(content::ExecuteScriptAndExtractString(
            WebContents::FromRenderViewHost(render_view_host),
            kGetFocusedElementJS, &focused_id));
        EXPECT_STREQ(kExpectedIDs[index], focused_id.c_str());
      }

      // On the last Tab key press, focus returns to the browser.
      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false,
                                                  reverse, false, false));

      // Except on Mac, where extra tabs are once again required to traverse the
      // other top chrome elements.
#if defined(OS_MACOSX)
      if (ui_controls::IsFullKeyboardAccessEnabled()) {
        for (int j = 0; j < (reverse ? kFocusableElementsAfterOmnibox
                                     : kFocusableElementsBeforeOmnibox);
             ++j) {
          ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false,
                                                      reverse, false, false));
        }
      }
#endif

      ui_test_utils::WaitForViewFocus(
          browser(), reverse ? VIEW_ID_OMNIBOX : VIEW_ID_LOCATION_ICON, true);

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false,
                                                  reverse, false, false));
      ui_test_utils::WaitForViewFocus(
          browser(), reverse ? VIEW_ID_LOCATION_ICON : VIEW_ID_OMNIBOX, true);
      if (reverse) {
        ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), key, false,
                                                    false, false, false));
      }
    }
  }
};

// A test interstitial page with typical HTML contents.
class TestInterstitialPage : public content::InterstitialPageDelegate {
 public:
  explicit TestInterstitialPage(WebContents* tab) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath file_path;
    bool success = base::PathService::Get(chrome::DIR_TEST_DATA, &file_path);
    EXPECT_TRUE(success);
    file_path = file_path.AppendASCII("focus/typical_page.html");
    success = base::ReadFileToString(file_path, &html_contents_);
    EXPECT_TRUE(success);
    interstitial_page_ = content::InterstitialPage::Create(
        tab, true, GURL("http://interstitial.com"), this);

    // Show the interstitial and delay return until it has attached.
    interstitial_page_->Show();
    content::WaitForInterstitialAttach(tab);

    EXPECT_TRUE(tab->ShowingInterstitialPage());
  }

  std::string GetHTMLContents() override { return html_contents_; }

  RenderViewHost* render_view_host() {
    return interstitial_page_->GetMainFrame()->GetRenderViewHost();
  }

  void DontProceed() { interstitial_page_->DontProceed(); }

  bool HasFocus() {
    return render_view_host()->GetWidget()->GetView()->HasFocus();
  }

 private:
  std::string html_contents_;
  content::InterstitialPage* interstitial_page_;  // Owns this.
  DISALLOW_COPY_AND_ASSIGN(TestInterstitialPage);
};

// Flaky on Mac (http://crbug.com/67301).
#if defined(OS_MACOSX)
#define MAYBE_ClickingMovesFocus DISABLED_ClickingMovesFocus
#else
// If this flakes, disable and log details in http://crbug.com/523255.
#define MAYBE_ClickingMovesFocus ClickingMovesFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_ClickingMovesFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#if defined(OS_POSIX)
  // It seems we have to wait a little bit for the widgets to spin up before
  // we can start clicking on them.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(),
      base::TimeDelta::FromMilliseconds(kActionDelayMs));
  content::RunMessageLoop();
#endif  // defined(OS_POSIX)

  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  ClickOnView(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  ClickOnView(VIEW_ID_OMNIBOX);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BrowsersRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeWindow();

  // The focus should be on the Tab contents.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  // Now hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  // Hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Tabs remember focus.
#if defined(THREAD_SANITIZER)
// TODO(crbug.com/995181): This test is flaky on Linux TSan.
#define MAYBE_TabsRememberFocus DISABLED_TabsRememberFocus
#else
#define MAYBE_TabsRememberFocus TabsRememberFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_TabsRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Create several tabs.
  for (int i = 0; i < 4; ++i) {
    chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  }

  // Alternate focus for the tab.
  const bool kFocusPage[3][5] = {{true, true, true, true, false},
                                 {false, false, false, false, false},
                                 {false, true, false, true, false}};

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->tab_strip_model()->ActivateTabAt(
          j, {TabStripModel::GestureType::kOther});

      // Activate the location bar or the page.
      if (kFocusPage[i][j]) {
        browser()->tab_strip_model()->GetWebContentsAt(j)->Focus();
      } else {
        chrome::FocusLocationBar(browser());
      }
    }

    // Now come back to the tab and check the right view is focused.
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->tab_strip_model()->ActivateTabAt(
          j, {TabStripModel::GestureType::kOther});

      ViewID vid = kFocusPage[i][j] ? VIEW_ID_TAB_CONTAINER : VIEW_ID_OMNIBOX;
      ASSERT_TRUE(IsViewFocused(vid));
    }

    browser()->tab_strip_model()->ActivateTabAt(
        0, {TabStripModel::GestureType::kOther});
    // Try the above, but with ctrl+tab. Since tab normally changes focus,
    // this has regressed in the past. Loop through several times to be sure.
    for (int j = 0; j < 15; j++) {
      ViewID vid =
          kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER : VIEW_ID_OMNIBOX;
      ASSERT_TRUE(IsViewFocused(vid));

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, true,
                                                  false, false, false));
    }

    // As above, but with ctrl+shift+tab.
    browser()->tab_strip_model()->ActivateTabAt(
        4, {TabStripModel::GestureType::kOther});
    for (int j = 14; j >= 0; --j) {
      ViewID vid =
          kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER : VIEW_ID_OMNIBOX;
      ASSERT_TRUE(IsViewFocused(vid));

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, true,
                                                  true, false, false));
    }
  }
}

// Tabs remember focus with find-in-page box.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabsRememberFocusFindInPage) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::Find(browser());
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16("a"), true, false, NULL, NULL);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Focus the location bar.
  chrome::FocusLocationBar(browser());

  // Create a 2nd tab.
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);

  // Focus should be on the recently opened tab page.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Select 1st tab, focus should still be on the location-bar.
  // (bug http://crbug.com/23296)
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Now open the find box again, switch to another tab and come back, the focus
  // should return to the find box.
  chrome::Find(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Background window does not steal focus.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BackgroundBrowserDontStealFocus) {
  // Ensure the browser process state is in sync with the WindowServer process.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open a new browser window.
  Browser* background_browser =
      new Browser(Browser::CreateParams(browser()->profile(), true));
  chrome::AddTabAt(background_browser, GURL(), -1, true);
  background_browser->window()->Show();

  const GURL steal_focus_url = embedded_test_server()->GetURL(kStealFocusPage);
  ui_test_utils::NavigateToURL(background_browser, steal_focus_url);

  // The navigation will activate |background_browser|. Except, on some
  // platforms, that may be asynchronous. Ensure the activation is properly
  // reflected in the browser process by activating again.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(background_browser));
  EXPECT_TRUE(background_browser->window()->IsActive());

  // Activate the first browser (again). Note BringBrowserWindowToFront() does
  // Show() and Focus(), but not Activate(), which is needed for Desktop Linux.
  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_TRUE(browser()->window()->IsActive());
  ASSERT_TRUE(content::ExecuteScript(
      background_browser->tab_strip_model()->GetActiveWebContents(),
      "stealFocus();"));

  // Try flushing tasks. Note that on Mac and Desktop Linux, window activation
  // is asynchronous. There's no way to guarantee that the WindowServer process
  // has actually activated a window without waiting for the activation event.
  // But this test is checking that _no_ activation event occurs. So there is
  // nothing to wait for. So, assuming the test fails and |unfocused_browser|
  // _did_ activate, the expectation below still isn't guaranteed to fail after
  // flushing run loops.
  content::RunAllTasksUntilIdle();

  // Make sure the first browser is still active.
  EXPECT_TRUE(browser()->window()->IsActive());
}

// Page cannot steal focus when focus is on location bar.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, LocationBarLockFocus) {
  // Open the page that steals focus.
  const GURL url = embedded_test_server()->GetURL(kStealFocusPage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::FocusLocationBar(browser());

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), "stealFocus();"));

  // Make sure the location bar is still focused.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Test forward and reverse focus traversal on a typical page.
// Disabled for Mac because it is flaky on "Mac10.9 Tests (dbg)",
// see https://crbug.com/60973.
#if defined(OS_MACOSX)
#define MAYBE_FocusTraversal DISABLED_FocusTraversal
#else
#define MAYBE_FocusTraversal FocusTraversal
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusTraversal) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  chrome::FocusLocationBar(browser());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(tab->GetRenderViewHost(), false));
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(tab->GetRenderViewHost(), true));
}

// Test forward and reverse focus traversal while an interstitial is showing.
// Disabled, see http://crbug.com/60973
IN_PROC_BROWSER_TEST_F(BrowserFocusTest,
                       DISABLED_FocusTraversalOnInterstitial) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Create and show a test interstitial page.
  TestInterstitialPage* interstitial_page = new TestInterstitialPage(
      browser()->tab_strip_model()->GetActiveWebContents());
  content::RenderViewHost* host = interstitial_page->render_view_host();

  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  chrome::FocusLocationBar(browser());
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(host, false));
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(host, true));
}

// Test the transfer of focus when an interstitial is shown and hidden.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, InterstitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(tab->GetRenderViewHost()->GetWidget()->GetView()->HasFocus());

  // Create and show a test interstitial page; it should gain focus.
  TestInterstitialPage* interstitial_page = new TestInterstitialPage(tab);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(interstitial_page->HasFocus());

  // Hide the interstitial; the original page should gain focus.
  interstitial_page->DontProceed();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(tab->GetRenderViewHost()->GetWidget()->GetView()->HasFocus());
}

// Test that find-in-page UI can request focus, even when it is already open.
#if defined(OS_MACOSX)
#define MAYBE_FindFocusTest DISABLED_FindFocusTest
#else
// If this flakes, disable and log details in http://crbug.com/523255.
#define MAYBE_FindFocusTest FindFocusTest
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FindFocusTest) {
  chrome::DisableFindBarAnimationsDuringTesting(true);
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ClickOnView(VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::Find(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Makes sure the focus is in the right location when opening the different
// types of tabs.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabInitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open the history tab, focus should be on the tab contents.
  chrome::ShowHistory(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open the new tab, focus should be on the location bar.
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Open the download tab, focus should be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open about:blank, focus should be on the location bar.
  chrome::AddSelectedTabWithURL(browser(), GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_LINK);
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Tests that focus goes where expected when using reload.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReload) {
  // Open the new tab, reload.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::NewTab(browser());
    observer.Wait();
  }
  content::RunAllPendingInMessageLoop();

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()
                 ->tab_strip_model()
                 ->GetActiveWebContents()
                 ->GetController()));
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  // Focus should stay on the location bar.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Open a regular page, focus the location bar, reload.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()
                 ->tab_strip_model()
                 ->GetActiveWebContents()
                 ->GetController()));
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }

  // Focus should now be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that focus goes where expected when using reload on a crashed tab.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// Hangy, http://crbug.com/50025.
#define MAYBE_FocusOnReloadCrashedTab DISABLED_FocusOnReloadCrashedTab
#else
#define MAYBE_FocusOnReloadCrashedTab FocusOnReloadCrashedTab
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusOnReloadCrashedTab) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open a regular page, crash, reload.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()
                 ->tab_strip_model()
                 ->GetActiveWebContents()
                 ->GetController()));
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }

  // Focus should now be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that focus goes to frame after crashed tab.
// TODO(shrikant): Find out where the focus should be deterministically.
// Currently focused_view after crash seem to be non null in debug mode
// (invalidated pointer 0xcccccc).
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_FocusAfterCrashedTab) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that when a new tab is opened from the omnibox, the focus is moved from
// the omnibox for the current tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, NavigateFromOmniboxIntoNewTab) {
  GURL url("http://www.google.com/");
  GURL url2("http://maps.google.com/");

  // Navigate to url.
  NavigateParams p(browser(), url, ui::PAGE_TRANSITION_LINK);
  p.window_action = NavigateParams::SHOW_WINDOW;
  p.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&p);

  // Focus the omnibox.
  chrome::FocusLocationBar(browser());

  OmniboxEditController* controller = browser()
                                          ->window()
                                          ->GetLocationBar()
                                          ->GetOmniboxView()
                                          ->model()
                                          ->controller();

  // Simulate an alt-enter.
  controller->OnAutocompleteAccept(
      url2, nullptr, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      base::TimeTicks());

  // Make sure the second tab is selected.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // The tab contents should have the focus in the second tab.
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Go back to the first tab. The focus should not be in the omnibox.
  chrome::SelectPreviousTab(browser());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Flaky on all platforms (http://crbug.com/665296).
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_FocusOnNavigate) {
  // Needed on Mac.
  // TODO(warx): check why it is needed on Mac.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // Load the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Navigate to another page.
  const base::FilePath::CharType* kEmptyFile = FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  ui_test_utils::NavigateToURL(browser(), file_url);

  ClickOnView(VIEW_ID_TAB_CONTAINER);

  // Navigate back.  Should focus the location bar.
  {
    content::WindowedNotificationObserver back_nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    back_nav_observer.Wait();
  }

  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Navigate forward.  Shouldn't focus the location bar.
  ClickOnView(VIEW_ID_TAB_CONTAINER);
  {
    content::WindowedNotificationObserver forward_nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
    forward_nav_observer.Wait();
  }

  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Ensure that crbug.com/567445 does not regress. This test checks that the
// Omnibox does not get focused when loading about:blank in a case where it's
// not the startup URL, e.g. when a page opens a popup to about:blank, with a
// null opener, and then navigates it. This is a potential security issue; see
// comments in |WebContentsImpl::FocusLocationBarByDefault|.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, AboutBlankNavigationLocationTest) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url1);

  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* web_contents = tab_strip->GetActiveWebContents();

  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  const std::string spoof =
      "var w = window.open('about:blank'); w.opener = null;"
      "w.document.location = '" +
      url2.spec() + "';";

  ASSERT_TRUE(content::ExecuteScript(web_contents, spoof));
  EXPECT_EQ(url1, web_contents->GetVisibleURL());
  // After running the spoof code, |GetActiveWebContents| returns the new tab,
  // not the same as |web_contents|.
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Regression test for https://crbug.com/677716.  This ensures that the omnibox
// does not get focused if another tab in the same window navigates to the New
// Tab Page, since that can scroll the origin of the selected tab out of view.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, NoFocusForBackgroundNTP) {
  // Start at the NTP and navigate to a test page.  We will later go back to the
  // NTP, which gives the omnibox focus in some cases.
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* opener_web_contents = tab_strip->GetActiveWebContents();

  // Open a second tab from the test page.
  const GURL new_url = embedded_test_server()->GetURL("/title2.html");
  const std::string open_script = "window.open('" + new_url.spec() + "');";
  content::WebContentsAddedObserver open_observer;
  ASSERT_TRUE(content::ExecuteScript(opener_web_contents, open_script));
  WebContents* new_web_contents = open_observer.GetWebContents();

  // Tell the first (non-selected) tab to go back.  This should not give the
  // omnibox focus, since the navigation occurred in a different tab.  Otherwise
  // the focus may scroll the origin out of view, making a spoof possible.
  const std::string go_back_script = "window.opener.history.back();";
  content::TestNavigationObserver back_observer(opener_web_contents);
  ASSERT_TRUE(content::ExecuteScript(new_web_contents, go_back_script));
  back_observer.Wait();
  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Tests that the location bar is focusable when showing, which is the case in
// popup windows.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, PopupLocationBar) {
  Browser* popup_browser = CreateBrowserForPopup(browser()->profile());

  // Make sure the popup is in the front. Otherwise the test is flaky.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(popup_browser));

  ui_test_utils::FocusView(popup_browser, VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(popup_browser, VIEW_ID_TAB_CONTAINER));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(popup_browser, ui::VKEY_TAB,
                                              false, false, false, false));
  ui_test_utils::WaitForViewFocus(popup_browser, VIEW_ID_LOCATION_ICON, true);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(popup_browser, ui::VKEY_TAB,
                                              false, false, false, false));
  ui_test_utils::WaitForViewFocus(popup_browser, VIEW_ID_OMNIBOX, true);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(popup_browser, ui::VKEY_TAB,
                                              false, false, false, false));
  ui_test_utils::WaitForViewFocus(popup_browser, VIEW_ID_TAB_CONTAINER, true);
}

// Tests that the location bar is not focusable when hidden, which is the case
// in app windows.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, AppLocationBar) {
  Browser* app_browser = CreateBrowserForApp("foo", browser()->profile());

  // Make sure the app window is in the front. Otherwise the test is flaky.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(app_browser));

  ui_test_utils::FocusView(app_browser, VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(app_browser, VIEW_ID_TAB_CONTAINER));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(app_browser, ui::VKEY_TAB, false,
                                              false, false, false));
  base::RunLoop().RunUntilIdle();
  ui_test_utils::WaitForViewFocus(app_browser, VIEW_ID_TAB_CONTAINER, true);
}

}  // namespace
