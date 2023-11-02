// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/page_navigator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SearchIPCRouterPolicyTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://blank"));
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
TEST_F(SearchIPCRouterPolicyTest, DoNotProcessFocusOmnibox) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessFocusOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendMostVisitedInfo) {
  // Send most visited items only if the current tab is an Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendMostVisitedInfo());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendNtpTheme) {
  // Send theme background information only if the current tab is an
  // Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendNtpTheme());
}
