// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kSearchMatchUrl1[] = "https://www.search-url-1.com/";
constexpr char kSearchMatchUrl2[] = "https://www.search-url-2.com/";
constexpr char kNonMatchUrl[] = "https://www.tab-frame-url.com/";

bool IsSearchURLMatch(const GURL& url) {
  return url == kSearchMatchUrl1 || url == kSearchMatchUrl2;
}

}  // namespace

namespace test {

class SideSearchTabContentsHelperTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSideSearch);
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SideSearchTabContentsHelper::CreateForWebContents(web_contents_.get());
    helper()->SetSidePanelContentsForTesting(
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr));
    // Basic configuration for testing that allows navigations to URLs matching
    // `kSearchMatchUrl1` and `kSearchMatchUrl2` to proceed within the side
    // panel and only allows showing the side panel on non-matching pages.
    auto* config = GetConfig();
    config->SetShouldNavigateInSidePanelCallback(
        base::BindRepeating(IsSearchURLMatch));
    config->SetCanShowSidePanelForURLCallback(base::BindRepeating(
        [](const GURL& url) { return !IsSearchURLMatch(url); }));
    config->SetGenerateSideSearchURLCallback(
        base::BindRepeating([](const GURL& url) { return url; }));
    Test::SetUp();
  }

 protected:
  void LoadURL(const char* url) {
    web_contents()->GetController().LoadURL(GURL(url), content::Referrer(),
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());
    CommitPendingNavigation();
  }

  void GoBack() {
    ASSERT_TRUE(web_contents()->GetController().CanGoBack());
    web_contents()->GetController().GoBack();
    CommitPendingNavigation();
  }

  void GoForward() {
    ASSERT_TRUE(web_contents()->GetController().CanGoForward());
    web_contents()->GetController().GoForward();
    CommitPendingNavigation();
  }

  void CommitPendingNavigation() {
    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

    if (side_contents() && side_contents()->GetController().GetPendingEntry()) {
      content::WebContentsTester::For(side_contents())
          ->CommitPendingNavigation();
    }
  }

  content::NavigationEntry* GetLastCommittedSideContentsEntry() {
    DCHECK(side_contents());
    return side_contents()->GetController().GetLastCommittedEntry();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  content::WebContents* side_contents() {
    return helper()->GetSidePanelContents();
  }

  SideSearchTabContentsHelper* helper() {
    return SideSearchTabContentsHelper::FromWebContents(web_contents_.get());
  }

  SideSearchConfig* GetConfig() { return SideSearchConfig::Get(&profile_); }

  void ResetWebContents() { web_contents_.reset(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40878321): Update this test to pass and re-enable.
TEST_F(SideSearchTabContentsHelperTest,
       DISABLED_LastSearchURLUpdatesCorrectly) {
  // When a tab is first opened there should be no last encountered search URL.
  EXPECT_FALSE(helper()->last_search_url().has_value());
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());

  // Navigating to a matching search URL should update the `last_search_url`.
  LoadURL(kSearchMatchUrl1);
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Navigating to a non-matching search URL should not change the
  // `last_search_url`.
  LoadURL(kNonMatchUrl);
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Navigating again to a new matching search url should update the
  // `last_search_url`.
  LoadURL(kSearchMatchUrl2);
  EXPECT_EQ(kSearchMatchUrl2, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl2, GetLastCommittedSideContentsEntry()->GetURL());

  // Going backwards to the non-matching search URL should not update the
  // `last_search_url`.
  GoBack();
  EXPECT_EQ(kSearchMatchUrl2, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl2, GetLastCommittedSideContentsEntry()->GetURL());

  // Going back to the original search URL should update the `last_search_url`.
  GoBack();
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Going forward to the non-matching search URL shouldn't change the
  // `last_search_url`.
  GoForward();
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Going forward to the latest matching search URL should update the
  // `last_search_url` to that latest URL.
  GoForward();
  EXPECT_EQ(kSearchMatchUrl2, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl2, GetLastCommittedSideContentsEntry()->GetURL());
}

// TODO(crbug.com/40878321): Update this test to pass and re-enable.
TEST_F(SideSearchTabContentsHelperTest,
       DISABLED_IndicatesWhenSidePanelShouldBeShown) {
  // With no initial navigation the side panel should not be showing.
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // If no previous matching search URL has been seen for this tab contents the
  // side panel should not show.
  LoadURL(kNonMatchUrl);
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // The side panel should not be visible on matching search pages.
  LoadURL(kSearchMatchUrl1);
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // If a matching page has previously been seen the side panel may be opened
  // on non-matching pages.
  LoadURL(kNonMatchUrl);
  EXPECT_TRUE(helper()->CanShowSidePanelForCommittedNavigation());
}

TEST_F(SideSearchTabContentsHelperTest, ClearsSidePanelContentsWhenAsked) {
  EXPECT_NE(nullptr, helper()->side_panel_contents_for_testing());
  helper()->ClearSidePanelContents();
  EXPECT_EQ(nullptr, helper()->side_panel_contents_for_testing());
}

// TODO(crbug.com/40878321): Update this test to pass and re-enable.
TEST_F(SideSearchTabContentsHelperTest, DISABLED_EmitsReturnedToSRPMetrics) {
  // Navigating to a matching search. Then navigate to a non-matching URL and
  // navigate back, doing so twice.
  LoadURL(kSearchMatchUrl1);
  LoadURL(kNonMatchUrl);
  GoBack();
  LoadURL(kNonMatchUrl);
  GoBack();
  LoadURL(kNonMatchUrl);

  // Return metrics should not yet have been emitted.
  histogram_tester().ExpectUniqueSample("SideSearch.TimesReturnedBackToSRP", 2,
                                        0);

  // Navigating to a new search URL should cause the previous metrics to have
  // been emitted.
  LoadURL(kSearchMatchUrl2);
  histogram_tester().ExpectUniqueSample("SideSearch.TimesReturnedBackToSRP", 2,
                                        1);

  // Navigating to a matching search. Then navigate to a non-matching URL and
  // navigate back, doing so three times.
  LoadURL(kNonMatchUrl);
  GoBack();
  LoadURL(kNonMatchUrl);
  GoBack();
  LoadURL(kNonMatchUrl);
  GoBack();
  LoadURL(kNonMatchUrl);

  // Return metrics for this interaction should not yet have been emitted.
  histogram_tester().ExpectBucketCount("SideSearch.TimesReturnedBackToSRP", 3,
                                       0);

  // Resetting the web contents should result in these metrics being emitted.
  ResetWebContents();
  histogram_tester().ExpectBucketCount("SideSearch.TimesReturnedBackToSRP", 3,
                                       1);
}

}  // namespace test
