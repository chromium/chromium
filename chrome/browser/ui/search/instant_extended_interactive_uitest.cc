// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class InstantExtendedTest : public InProcessBrowserTest,
                            public InstantTestBase {
 public:
  InstantExtendedTest() = default;

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_test_server().Start());
    GURL base_url = https_test_server().GetURL("/instant_extended.html?");
    GURL ntp_url = https_test_server().GetURL("/instant_extended_ntp.html?");
    ASSERT_NO_FATAL_FAILURE(
        SetupInstant(browser()->profile(), base_url, ntp_url));
  }

  void UpdateSearchState(content::WebContents* contents) {
    on_most_visited_change_calls_ =
        content::EvalJs(contents, "onMostVisitedChangedCalls").ExtractInt();
  }

  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetOmniboxView();
  }

  void FocusOmnibox() {
    // If the omnibox already has focus, just notify OmniboxTabHelper.
    if (omnibox()->model()->has_focus()) {
      content::WebContents* active_tab =
          browser()->tab_strip_model()->GetActiveWebContents();
      OmniboxTabHelper::FromWebContents(active_tab)
          ->OnFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
    } else {
      browser()->window()->GetLocationBar()->FocusLocation(false);
    }
  }

  void SetOmniboxText(const std::string& text) {
    FocusOmnibox();
    omnibox()->SetUserText(base::UTF8ToUTF16(text));
  }

  void PressEnterAndWaitForLoadStop() {
    content::TestNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    browser()
        ->window()
        ->GetLocationBar()
        ->GetOmniboxView()
        ->model()
        ->OpenSelection();
    observer.Wait();
  }

  int on_most_visited_change_calls_ = 0;
};

// Test to verify that switching tabs should not dispatch onmostvisitedchanged
// events.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoMostVisitedChangedOnTabSwitch) {
  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Make sure new tab received the onmostvisitedchanged event once.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  UpdateSearchState(active_tab);
  EXPECT_EQ(1, on_most_visited_change_calls_);

  // Activate the previous tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Switch back to new tab.
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Confirm that new tab got no onmostvisitedchanged event.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  UpdateSearchState(active_tab);
  EXPECT_EQ(1, on_most_visited_change_calls_);
}

// TODO(crbug.com/40810214): Failing on MSan.
#if defined(MEMORY_SANITIZER)
#define MAYBE_NavigateBackToNTP DISABLED_NavigateBackToNTP
#else
#define MAYBE_NavigateBackToNTP NavigateBackToNTP
#endif
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, MAYBE_NavigateBackToNTP) {
  FocusOmnibox();

  // Open a new tab page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  SetOmniboxText("flowers");
  PressEnterAndWaitForLoadStop();

  // Navigate back to NTP.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::TestNavigationObserver back_observer(active_tab);
  active_tab->GetController().GoBack();
  back_observer.Wait();

  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(search::IsInstantNTP(active_tab));
}

// TODO(crbug.com/40810214): Failing on MSan.
#if defined(MEMORY_SANITIZER)
#define MAYBE_DispatchMVChangeEventWhileNavigatingBackToNTP \
  DISABLED_DispatchMVChangeEventWhileNavigatingBackToNTP
#else
#define MAYBE_DispatchMVChangeEventWhileNavigatingBackToNTP \
  DispatchMVChangeEventWhileNavigatingBackToNTP
#endif
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_DispatchMVChangeEventWhileNavigatingBackToNTP) {
  FocusOmnibox();

  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  UpdateSearchState(active_tab);
  EXPECT_EQ(1, on_most_visited_change_calls_);

  // Set the text and press enter to navigate from NTP.
  SetOmniboxText("Pen");
  PressEnterAndWaitForLoadStop();

  // Navigate back to NTP.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::TestNavigationObserver back_observer(active_tab);
  active_tab->GetController().GoBack();
  back_observer.Wait();

  // Verify that onmostvisitedchange event is dispatched when we navigate from
  // SRP to NTP.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  UpdateSearchState(active_tab);
  EXPECT_EQ(1, on_most_visited_change_calls_);
}

// Check that clicking on a result sends the correct referrer.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, Referrer) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL result_url = embedded_test_server()->GetURL(
      "/referrer_policy/referrer-policy-log.html");
  FocusOmnibox();

  // Type a query and press enter to get results.
  SetOmniboxText("query");
  PressEnterAndWaitForLoadStop();

  // Simulate going to a result.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURLFromRenderer(contents, result_url));

  EXPECT_TRUE(content::WaitForLoadStop(contents));
  std::string expected_title =
      "Referrer is " + https_test_server().base_url().spec();
  EXPECT_EQ(base::ASCIIToUTF16(expected_title), contents->GetTitle());
}
