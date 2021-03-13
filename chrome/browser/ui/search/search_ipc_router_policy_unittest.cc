// Copyright 2013 The Chromium Authors. All rights reserved.
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

TEST_F(SearchIPCRouterPolicyTest, ProcessFocusOmnibox) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessFocusOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessFocusOmnibox) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessFocusOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, ProcessDeleteMostVisitedItem) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(
      GetSearchIPCRouterPolicy()->ShouldProcessDeleteMostVisitedItem());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessUndoMostVisitedDeletion) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(
      GetSearchIPCRouterPolicy()->ShouldProcessUndoMostVisitedDeletion());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessUndoAllMostVisitedDeletions) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(
      GetSearchIPCRouterPolicy()->ShouldProcessUndoAllMostVisitedDeletions());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessLogEvent) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessLogEvent());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessLogEvent) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessLogEvent());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessPasteIntoOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessPasteIntoOmniboxMsg) {
  // Process message only if the current tab is an Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessMessagesForIncognitoPage) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetIncognitoProfile();

  SearchIPCRouter::Policy* router_policy = GetSearchIPCRouterPolicy();
  EXPECT_FALSE(router_policy->ShouldProcessFocusOmnibox(true));
  EXPECT_FALSE(router_policy->ShouldProcessDeleteMostVisitedItem());
  EXPECT_FALSE(router_policy->ShouldProcessUndoMostVisitedDeletion());
  EXPECT_FALSE(router_policy->ShouldProcessUndoAllMostVisitedDeletions());
  EXPECT_FALSE(router_policy->ShouldProcessLogEvent());
  EXPECT_FALSE(router_policy->ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessMessagesForInactiveTab) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));

  // Assume the NTP is deactivated.
  SearchIPCRouter::Policy* router_policy = GetSearchIPCRouterPolicy();
  EXPECT_FALSE(router_policy->ShouldProcessFocusOmnibox(false));
  EXPECT_FALSE(router_policy->ShouldProcessPasteIntoOmnibox(false));
  EXPECT_FALSE(router_policy->ShouldSendSetInputInProgress(false));
}

TEST_F(SearchIPCRouterPolicyTest,
       DoNotSendSetMessagesForIncognitoPage) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetIncognitoProfile();

  SearchIPCRouter::Policy* router_policy = GetSearchIPCRouterPolicy();
  EXPECT_FALSE(router_policy->ShouldSendNtpTheme());
  EXPECT_FALSE(router_policy->ShouldSendMostVisitedInfo());
  EXPECT_FALSE(router_policy->ShouldSendSetInputInProgress(true));
  EXPECT_FALSE(router_policy->ShouldSendOmniboxFocusChanged());
}

TEST_F(SearchIPCRouterPolicyTest, SendMostVisitedInfo) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendMostVisitedInfo());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendMostVisitedInfo) {
  // Send most visited items only if the current tab is an Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendMostVisitedInfo());
}

TEST_F(SearchIPCRouterPolicyTest, SendNtpTheme) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendNtpTheme());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendNtpTheme) {
  // Send theme background information only if the current tab is an
  // Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendNtpTheme());
}
