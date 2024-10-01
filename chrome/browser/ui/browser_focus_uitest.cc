// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/focus_changed_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/test/scoped_feature_list.h"
#endif

namespace {

constexpr char kGetFocusedElementJS[] = "getFocusedElement();";

// Listens to UI and DOM element focus changes.
class FocusChangeObserver : public views::FocusChangeListener,
                            public content::WebContentsObserver {
 public:
  FocusChangeObserver(views::FocusManager* focus_manager,
                      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {
    obs_.Observe(focus_manager);
  }

  void WaitForFocusChange() { run_loop_.Run(); }

  // FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    if (focused_now) {
      SCOPED_TRACE(base::StrCat(
          {"View with ID=", base::NumberToString(focused_now->GetID()),
           " is focused now."}));
    }
    run_loop_.Quit();
  }

  // WebContentsObserver:
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override {
    SCOPED_TRACE(base::StrCat(
        {"Page element with id=",
         content::EvalJs(web_contents(), kGetFocusedElementJS).ExtractString(),
         " is focused now."}));
    run_loop_.Quit();
  }

 private:
  base::ScopedObservation<views::FocusManager, FocusChangeObserver> obs_{this};
  base::RunLoop run_loop_;
};

}  // namespace

namespace base {

template <>
struct ScopedObservationTraits<views::FocusManager, FocusChangeObserver> {
  static void AddObserver(views::FocusManager* source,
                          FocusChangeObserver* observer) {
    source->AddFocusChangeListener(observer);
  }
  static void RemoveObserver(views::FocusManager* source,
                             FocusChangeObserver* observer) {
    source->RemoveFocusChangeListener(observer);
  }
};

}  // namespace base

namespace {

using content::RenderViewHost;
using content::WebContents;

const char kSimplePage[] = "/focus/page_with_focus.html";
const char kStealFocusPage[] = "/focus/page_steals_focus.html";
const char kTypicalPage[] = "/focus/typical_page.html";

class BrowserFocusBasicTest : public InProcessBrowserTest {
 public:
  BrowserFocusBasicTest() {
    // interactive_ui_tests set `ui_test_utils::BringBrowserWindowToFront()` for
    // the setup function, which interferes with what the test wants to test so
    // unset it.
    set_global_browser_set_up_function(nullptr);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
    // For CHROME_HEADLESS, which is currently used for browser tests, native
    // window occlusion is turned off. Turn it on to match the production
    // environment.
    base::FieldTrialParams field_trial_params{
        { features::kApplyNativeOcclusionToCompositorType.name,
          features::kApplyNativeOcclusionToCompositorTypeRelease }};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kApplyNativeOcclusionToCompositor, field_trial_params},
         { features::kAlwaysTrackNativeWindowOcclusionForTest,
           {} }},
        /*disabled_features=*/{});
#endif
  }

  views::Widget* GetWidgetForBrowser(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    CHECK(browser_view);
    views::Widget* widget = browser_view->GetWidget();
    CHECK(widget);
    return widget;
  }

  bool IsBrowserActive(Browser* browser) {
    return GetWidgetForBrowser(browser)->IsActive();
  }

 private:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

// A basic test to check that a newly opened browser window has focus and the
// focus is on the omnibox.
IN_PROC_BROWSER_TEST_F(BrowserFocusBasicTest, BrowserFocusedOnCreation) {
  // Ensure that the initialization of the browser window is completed.
  ui_test_utils::CreateAsyncWidgetRequestWaiter(*browser()).Wait();
  // Widget activation happens asynchronously after window creation on some
  // platforms like Linux so absorb the difference by waiting for the
  // activation.
  views::test::WaitForWidgetActive(GetWidgetForBrowser(browser()), true);
  // Check that when a browser is created, it's active.
  EXPECT_TRUE(IsBrowserActive(browser()));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Use `chrome::OpenEmptyWindow()` instead of directly creating a `Browser`
  // instance with `Browser::Create()` and calling `BrowserView::Show()` like
  // some tests do because this is what the production code does when opening a
  // new window. The difference is that it makes sure that there is at least one
  // tab on the window before calling `BrowserView::Show()`.
  Browser* browser2 = chrome::OpenEmptyWindow(browser()->profile());
  ui_test_utils::CreateAsyncWidgetRequestWaiter(*browser2).Wait();
  views::test::WaitForWidgetActive(GetWidgetForBrowser(browser2), true);
  EXPECT_TRUE(IsBrowserActive(browser2));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser2, VIEW_ID_OMNIBOX));
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

class BrowserFocusTest : public InteractiveBrowserTest {
 public:
  // InteractiveBrowserTest overrides:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Slow bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

  void ClickOnView(ViewID vid) { ui_test_utils::ClickOnView(browser(), vid); }

  void FocusNextElement(bool reverse) {
    FocusChangeObserver obs{
        GetFocusManager(),
        browser()->tab_strip_model()->GetActiveWebContents()};
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                                reverse, false, false));
    obs.WaitForFocusChange();
  }

  void TestFocusTraversal(bool reverse) {
    SCOPED_TRACE(base::StrCat(
        {"Started focus traversal, reverse=", base::ToString(reverse)}));

    // Move focus one element away from the omnibox.
    ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
    FocusNextElement(reverse);
    EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));

    // Traverse the whole focus chain until the omnibox is focused again.
    size_t c = 0;
    while (!IsViewFocused(VIEW_ID_OMNIBOX) && c < kMaxIterations) {
      FocusNextElement(reverse);
      ++c;
    }
    EXPECT_TRUE(c <= kMaxIterations);
    EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  }

  views::FocusManager* GetFocusManager() {
    BrowserWindow* browser_window = browser()->window();
    DCHECK(browser_window);
    gfx::NativeWindow window = browser_window->GetNativeWindow();
    DCHECK(window);
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    DCHECK(widget);
    views::FocusManager* focus_manager = widget->GetFocusManager();
    DCHECK(focus_manager);
    return focus_manager;
  }

  views::Widget* GetWidgetForBrowser(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    CHECK(browser_view);
    views::Widget* widget = browser_view->GetWidget();
    CHECK(widget);
    return widget;
  }

 private:
  constexpr static size_t kMaxIterations = 20;
};

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, ClickingMovesFocus) {
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      ObserveState(views::test::kCurrentFocusedViewId,
                   GetWidgetForBrowser(browser())),
      WaitForState(views::test::kCurrentFocusedViewId, kOmniboxElementId),
      // Click on the tab container and check that it has focus.
      MoveMouseTo(ContentsWebView::kContentsWebViewElementId), ClickMouse(),
      WaitForState(views::test::kCurrentFocusedViewId,
                   ContentsWebView::kContentsWebViewElementId),
      // Click on the omnibox and check that it has focus.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      WaitForState(views::test::kCurrentFocusedViewId, kOmniboxElementId));
}

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BrowsersRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabsRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
          j, TabStripUserGestureDetails(
                 TabStripUserGestureDetails::GestureType::kOther));

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
          j, TabStripUserGestureDetails(
                 TabStripUserGestureDetails::GestureType::kOther));

      ViewID vid = kFocusPage[i][j] ? VIEW_ID_TAB_CONTAINER : VIEW_ID_OMNIBOX;
      ASSERT_TRUE(IsViewFocused(vid));
    }

    browser()->tab_strip_model()->ActivateTabAt(
        0, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
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
        4, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
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
  // TODO(crbug.com/40268465): Re-enable when child widget focus manager
  // relationship is fixed.
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSMajorVersion() >= 13) {
    GTEST_SKIP() << "Broken on macOS 13: https://crbug.com/1446127";
  }
#endif
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), u"a", true, false,
      nullptr, nullptr);
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
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Now open the find box again, switch to another tab and come back, the focus
  // should return to the find box.
  chrome::Find(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Background window does not steal focus.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BackgroundBrowserDontStealFocus) {
  // Ensure the browser process state is in sync with the WindowServer process.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open a new browser window.
  Browser* background_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::AddTabAt(background_browser, GURL(), -1, true);
  background_browser->window()->Show();

  const GURL steal_focus_url = embedded_test_server()->GetURL(kStealFocusPage);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(background_browser, steal_focus_url));

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
  ASSERT_TRUE(content::ExecJs(
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::FocusLocationBar(browser());

  ASSERT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(), "stealFocus();"));

  // Make sure the location bar is still focused.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Test forward and reverse focus traversal on a typical page.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusTraversal) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kTypicalPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  FocusChangeObserver obs{GetFocusManager(),
                          browser()->tab_strip_model()->GetActiveWebContents()};
  chrome::FocusLocationBar(browser());
  obs.WaitForFocusChange();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  // Loop through the focus chain twice in each direction for good measure.
  TestFocusTraversal(false);
  TestFocusTraversal(false);
  TestFocusTraversal(true);
  TestFocusTraversal(true);
}

// Test that find-in-page UI can request focus, even when it is already open.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FindFocusTest) {
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      ObserveState(views::test::kCurrentFocusedViewId,
                   GetWidgetForBrowser(browser())),
      Do([this]() { chrome::Find(browser()); }),
      WaitForState(views::test::kCurrentFocusedViewId, FindBarView::kTextField),
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      WaitForState(views::test::kCurrentFocusedViewId, kOmniboxElementId),
      Do([this]() { chrome::Find(browser()); }),
      WaitForState(views::test::kCurrentFocusedViewId,
                   FindBarView::kTextField));
}

// Makes sure the focus is in the right location when opening the different
// types of tabs.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabInitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open the history tab, focus should be on the tab contents.
  chrome::ShowHistory(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open the new tab, focus should be on the location bar.
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Open the download tab, focus should be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open about:blank, focus should be on the location bar.
  chrome::AddSelectedTabWithURL(browser(), GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_LINK);
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Tests that focus goes where expected when using reload.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReload) {
  // Open the new tab, reload.
  {
    auto& contents = chrome::NewTab(browser());
    content::WaitForLoadStop(&contents);
  }
  content::RunAllPendingInMessageLoop();

  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  // Focus should stay on the location bar.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Open a regular page, focus the location bar, reload.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));
  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }

  // Focus should now be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that focus goes where expected when using reload on a crashed tab.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Hangy, http://crbug.com/50025.
#define MAYBE_FocusOnReloadCrashedTab DISABLED_FocusOnReloadCrashedTab
#else
#define MAYBE_FocusOnReloadCrashedTab FocusOnReloadCrashedTab
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusOnReloadCrashedTab) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open a regular page, crash, reload.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimplePage)));
  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
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

// Tests that when omnibox triggers a navigation, then the focus is moved into
// the current tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, NavigateFromOmnibox) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  // Focus the Omnibox.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  chrome::FocusLocationBar(browser());
  OmniboxView* view = browser()->window()->GetLocationBar()->GetOmniboxView();

  // Simulate typing a URL into the omnibox.
  view->SetUserText(base::UTF8ToUTF16(url.spec()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  EXPECT_FALSE(view->IsSelectAll());

  // Simulate pressing Enter and wait until the navigation starts.
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  content::TestNavigationManager nav_manager(web_contents, url);
  ASSERT_TRUE(ui_controls::SendKeyPress(browser()->window()->GetNativeWindow(),
                                        ui::VKEY_RETURN, false, false, false,
                                        false));
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  // Verify that a navigation has started.
  EXPECT_TRUE(web_contents->GetController().GetPendingEntry());
  // Verify that the Omnibox text is not selected - this is a regression test
  // for https://crbug.com/1048742.
  EXPECT_FALSE(view->IsSelectAll());
  // Intentionally not asserting anything about IsViewFocused in this
  // _intermediate_ state.

  // Wait for the navigation to finish and verify final, steady state.
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_manager.was_successful());
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(view->IsSelectAll());
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

  OmniboxClient* omnibox_client = browser()
                                      ->window()
                                      ->GetLocationBar()
                                      ->GetOmniboxView()
                                      ->controller()
                                      ->client();

  // Simulate an alt-enter.
  omnibox_client->OnAutocompleteAccept(
      url2, nullptr, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      base::TimeTicks(), false, false, std::u16string(), AutocompleteMatch(),
      AutocompleteMatch(), IDNA2008DeviationCharacter::kNone);

  // Make sure the second tab is selected.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // The tab contents should have the focus in the second tab.
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Go back to the first tab. The focus should not be in the omnibox.
  chrome::SelectPreviousTab(browser());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, OmniboxFocusesOnNewTab) {
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      ObserveState(views::test::kCurrentFocusedViewId,
                   GetWidgetForBrowser(browser())),
      WaitForState(views::test::kCurrentFocusedViewId, kOmniboxElementId));
}

// TODO(crbug.com/370117091): This currently tests the incorrect behavior that
// causes the omnibox to *not* get focus when going back to a new tab page via
// the history. Update this test when the bug is fixed.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, OmniboxFocusStateAcrossHistory) {
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      ObserveState(views::test::kCurrentFocusedViewId,
                   GetWidgetForBrowser(browser())),
      NavigateWebContents(kWebContentsId, GURL(chrome::kChromeUINewTabURL)),
      NavigateWebContents(kWebContentsId,
                          embedded_test_server()->GetURL("/title1.html")),
      MoveMouseTo(ContentsWebView::kContentsWebViewElementId), ClickMouse(),
      // Navigate back. Check that the location bar is not focused. This should
      // focus the location bar, but that is not the current behavior.
      PressButton(kToolbarBackButtonElementId),
      WaitForWebContentsNavigation(kWebContentsId,
                                   GURL(chrome::kChromeUINewTabURL)),
      WaitForState(views::test::kCurrentFocusedViewId,
                   testing::Ne(kOmniboxElementId)),
      // Navigate forward. Should focus the body.
      PressButton(kToolbarForwardButtonElementId),
      WaitForWebContentsNavigation(
          kWebContentsId, embedded_test_server()->GetURL("/title1.html")),
      WaitForState(views::test::kCurrentFocusedViewId,
                   ContentsWebView::kContentsWebViewElementId));
}

// Ensure that crbug.com/567445 does not regress. This test checks that the
// Omnibox does not get focused when loading about:blank in a case where it's
// not the startup URL, e.g. when a page opens a popup to about:blank, with a
// null opener, and then navigates it. This is a potential security issue; see
// comments in |WebContentsImpl::FocusLocationBarByDefault|.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, AboutBlankNavigationLocationTest) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* web_contents = tab_strip->GetActiveWebContents();

  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  const std::string spoof =
      "var w = window.open('about:blank'); w.opener = null;"
      "w.document.location = '" +
      url2.spec() + "';";

  ASSERT_TRUE(content::ExecJs(web_contents, spoof));
  EXPECT_EQ(url1, web_contents->GetVisibleURL());
  // After running the spoof code, |GetActiveWebContents| returns the new tab,
  // not the same as |web_contents|.
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Regression test for https://crbug.com/677716.  This ensures that the omnibox
// does not get focused if another tab in the same window navigates to the New
// Tab Page, since that can scroll the origin of the selected tab out of view.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, NoFocusForBackgroundNTP) {
  // Start at the NTP and navigate to a test page.  We will later go back to the
  // NTP, which gives the omnibox focus in some cases.
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* opener_web_contents = tab_strip->GetActiveWebContents();

  // Open a second tab from the test page.
  const GURL new_url = embedded_test_server()->GetURL("/title2.html");
  const std::string open_script = "window.open('" + new_url.spec() + "');";
  content::WebContentsAddedObserver open_observer;
  ASSERT_TRUE(content::ExecJs(opener_web_contents, open_script));
  WebContents* new_web_contents = open_observer.GetWebContents();

  // Tell the first (non-selected) tab to go back.  This should not give the
  // omnibox focus, since the navigation occurred in a different tab.  Otherwise
  // the focus may scroll the origin out of view, making a spoof possible.
  const std::string go_back_script = "window.opener.history.back();";
  content::TestNavigationObserver back_observer(opener_web_contents);
  ASSERT_TRUE(content::ExecJs(new_web_contents, go_back_script));
  back_observer.Wait();
  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Tests that the location bar is focusable when showing, which is the case in
// popup windows.
// TODO(crbug.com/40794922): Flaky on Linux.
// TODO(crbug/1520655): Broken since CR2023.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_PopupLocationBar) {
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
  if (sharing_hub::HasPageAction(browser()->profile(), true)) {
    ui_test_utils::WaitForViewFocus(popup_browser, VIEW_ID_SHARING_HUB_BUTTON,
                                    true);
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(popup_browser, ui::VKEY_TAB,
                                                false, false, false, false));
  }

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
