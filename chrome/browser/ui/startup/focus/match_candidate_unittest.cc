// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/match_candidate.h"

#include <memory>
#include <vector>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/startup/focus/selector.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace focus {

class MatchCandidateTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ =
        std::make_unique<TabStripModel>(delegate_.get(), profile_.get());

    ON_CALL(browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    delegate_->SetBrowserWindowInterface(&browser_window_interface_);
  }

  void TearDown() override { delegate_->SetBrowserWindowInterface(nullptr); }

  std::unique_ptr<content::WebContents> CreateWebContents(const GURL& url) {
    auto contents = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    content::WebContentsTester::For(contents.get())->NavigateAndCommit(url);
    return contents;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestTabStripModelDelegate> delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  MockBrowserWindowInterface browser_window_interface_;
};

// Test MatchCandidate sorting by MRU (Most Recently Used).
TEST_F(MatchCandidateTest, MRUSorting) {
  GURL url1("https://example.com");
  GURL url2("https://test.com");

  auto web_contents1 = CreateWebContents(url1);
  auto web_contents2 = CreateWebContents(url2);

  base::Time old_time = base::Time::Now() - base::Seconds(10);
  base::Time recent_time = base::Time::Now();

  // Create two regular tab candidates with different last active times.
  MatchCandidate old_candidate(browser_window_interface_, 0, *web_contents1,
                               old_time, url1.spec(), std::nullopt);
  MatchCandidate recent_candidate(browser_window_interface_, 1, *web_contents2,
                                  recent_time, url2.spec(), std::nullopt);

  // Recent candidate should come first in MRU sorting (be "less than").
  EXPECT_TRUE(recent_candidate < old_candidate);
  EXPECT_FALSE(old_candidate < recent_candidate);
}

// Test that app windows have priority over regular tabs.
TEST_F(MatchCandidateTest, AppWindowPriority) {
  GURL url1("https://example.com");
  GURL url2("https://app.com");

  auto web_contents1 = CreateWebContents(url1);
  auto web_contents2 = CreateWebContents(url2);

  base::Time same_time = base::Time::Now();

  // Create a regular tab and an app window with the same last active time.
  MatchCandidate regular_tab(browser_window_interface_, 0, *web_contents1,
                             same_time, url1.spec(), std::nullopt);
  MatchCandidate app_window(browser_window_interface_, 0, *web_contents2,
                            same_time, url2.spec(), "com.example.app");

  // App window should come first (be "less than") regular tab.
  EXPECT_TRUE(app_window < regular_tab);
  EXPECT_FALSE(regular_tab < app_window);
}

// Test tab index tiebreaker for regular tabs with same time.
TEST_F(MatchCandidateTest, TabIndexTiebreaker) {
  GURL url1("https://example.com");
  GURL url2("https://test.com");

  auto web_contents1 = CreateWebContents(url1);
  auto web_contents2 = CreateWebContents(url2);

  base::Time same_time = base::Time::Now();

  // Create two regular tabs with same time but different indices.
  MatchCandidate tab_index_0(browser_window_interface_, 0, *web_contents1,
                             same_time, url1.spec(), std::nullopt);
  MatchCandidate tab_index_2(browser_window_interface_, 2, *web_contents2,
                             same_time, url2.spec(), std::nullopt);

  // Lower tab index should come first.
  EXPECT_TRUE(tab_index_0 < tab_index_2);
  EXPECT_FALSE(tab_index_2 < tab_index_0);
}

// Test that URL canonicalization works through MatchTab.
TEST_F(MatchCandidateTest, UrlCanonicalizationThroughMatchTab) {
  // Test 1: URLs with trailing slashes match URLs without them.
  GURL tab_url_with_slash("https://example.com/path/");
  GURL selector_url_without_slash("https://example.com/path");

  auto web_contents = CreateWebContents(tab_url_with_slash);
  Selector exact_selector(SelectorType::kUrlExact, selector_url_without_slash);

  // Should match despite the trailing slash difference due to canonicalization.
  auto match =
      MatchTab(exact_selector, browser_window_interface_, 0, *web_contents);
  ASSERT_TRUE(match.has_value());
  EXPECT_EQ(tab_url_with_slash.spec(), match->matched_url);

  // Test 2: The reverse - tab without slash, selector with slash.
  GURL tab_url_without_slash("https://example.com/other");
  GURL selector_url_with_slash("https://example.com/other/");

  auto web_contents2 = CreateWebContents(tab_url_without_slash);
  Selector exact_selector2(SelectorType::kUrlExact, selector_url_with_slash);

  auto match2 =
      MatchTab(exact_selector2, browser_window_interface_, 0, *web_contents2);
  ASSERT_TRUE(match2.has_value());
  EXPECT_EQ(tab_url_without_slash.spec(), match2->matched_url);

  // Test 3: Root path should keep trailing slash and still match.
  GURL root_url_with_slash("https://example.com/");
  GURL root_url_also_with_slash("https://example.com/");

  auto web_contents3 = CreateWebContents(root_url_with_slash);
  Selector exact_selector3(SelectorType::kUrlExact, root_url_also_with_slash);

  auto match3 =
      MatchTab(exact_selector3, browser_window_interface_, 0, *web_contents3);
  ASSERT_TRUE(match3.has_value());
  EXPECT_EQ("https://example.com/", match3->matched_url);

  // Test 4: URLs that already match don't need canonicalization.
  GURL matching_url("https://example.com/exact");

  auto web_contents4 = CreateWebContents(matching_url);
  Selector exact_selector4(SelectorType::kUrlExact, matching_url);

  auto match4 =
      MatchTab(exact_selector4, browser_window_interface_, 0, *web_contents4);
  ASSERT_TRUE(match4.has_value());
  EXPECT_EQ(matching_url.spec(), match4->matched_url);
}

// Test MatchTab function with exact URL matching.
TEST_F(MatchCandidateTest, MatchTabExactUrl) {
  GURL test_url("https://example.com/path");
  auto web_contents = CreateWebContents(test_url);

  Selector exact_selector(SelectorType::kUrlExact, test_url);

  auto match =
      MatchTab(exact_selector, browser_window_interface_, 0, *web_contents);

  ASSERT_TRUE(match.has_value());
  EXPECT_EQ(test_url.spec(), match->matched_url);
  EXPECT_FALSE(match->app_id.has_value());
  EXPECT_EQ(0, match->tab_index);
}

// Test MatchTab function with prefix URL matching.
TEST_F(MatchCandidateTest, MatchTabPrefixUrl) {
  GURL tab_url("https://example.com/path/subpath");
  GURL prefix_url("https://example.com/path");
  auto web_contents = CreateWebContents(tab_url);

  Selector prefix_selector(SelectorType::kUrlPrefix, prefix_url);

  auto match =
      MatchTab(prefix_selector, browser_window_interface_, 1, *web_contents);

  ASSERT_TRUE(match.has_value());
  EXPECT_EQ(tab_url.spec(), match->matched_url);
  EXPECT_FALSE(match->app_id.has_value());
  EXPECT_EQ(1, match->tab_index);
}

// Test MatchTab function returns nullopt for non-matching URLs.
TEST_F(MatchCandidateTest, MatchTabNoMatch) {
  GURL tab_url("https://example.com/path");
  GURL different_url("https://other.com/path");
  auto web_contents = CreateWebContents(tab_url);

  Selector selector(SelectorType::kUrlExact, different_url);

  auto match = MatchTab(selector, browser_window_interface_, 0, *web_contents);

  EXPECT_FALSE(match.has_value());
}

}  // namespace focus
