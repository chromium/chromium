// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class SearchIPCRouterPolicyBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://blank"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    SearchTabHelper::CreateForWebContents(web_contents());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SearchIPCRouter::Policy* GetSearchIPCRouterPolicy() {
    SearchTabHelper* search_tab_helper =
        SearchTabHelper::FromWebContents(web_contents());
    EXPECT_NE(nullptr, search_tab_helper);
    return search_tab_helper->ipc_router_for_testing().policy_for_testing();
  }

  void SetIncognitoProfile() {
    SearchIPCRouterPolicyImpl* policy =
        static_cast<SearchIPCRouterPolicyImpl*>(GetSearchIPCRouterPolicy());
    policy->set_is_incognito(true);
  }
};

// TODO(aee): ensure tests are added for public portions of the NTP API for
// remote NTPs.
IN_PROC_BROWSER_TEST_F(SearchIPCRouterPolicyBrowserTest,
                       DoNotProcessFocusOmnibox) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateParams params(browser(), GURL("chrome-search://foo/bar"),
                        ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessFocusOmnibox(true));
}

IN_PROC_BROWSER_TEST_F(SearchIPCRouterPolicyBrowserTest,
                       DoNotSendMostVisitedInfo) {
  // Send most visited items only if the current tab is an Instant NTP.
  NavigateParams params(browser(), GURL("chrome-search://foo/bar"),
                        ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendMostVisitedInfo());
}

IN_PROC_BROWSER_TEST_F(SearchIPCRouterPolicyBrowserTest, DoNotSendNtpTheme) {
  // Send theme background information only if the current tab is an
  // Instant NTP.
  NavigateParams params(browser(), GURL("chrome-search://foo/bar"),
                        ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendNtpTheme());
}
