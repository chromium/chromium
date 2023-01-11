// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
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

constexpr char kSearchMatchUrl[] = "https://www.search-url.com/";
constexpr char kNonMatchUrl[] = "https://www.tab-frame-url.com/";

class MockSideContentsDelegate : public SideSearchSideContentsHelper::Delegate {
 public:
  MockSideContentsDelegate() = default;
  MockSideContentsDelegate(const MockSideContentsDelegate&) = delete;
  MockSideContentsDelegate& operator=(const MockSideContentsDelegate&) = delete;
  ~MockSideContentsDelegate() = default;

  // SideSearchSideContentsHelper::Delegate:
  void NavigateInTabContents(const content::OpenURLParams& params) override {
    tab_contents_url_ = params.url;
    ++navigate_in_tab_contents_times_called_;
  }
  void LastSearchURLUpdated(const GURL& url) override {
    last_search_url_ = url;
  }
  void SidePanelProcessGone() override {}
  content::WebContents* GetTabWebContents() override { return nullptr; }
  void CarryOverSideSearchStateToNewTab(
      const GURL& search_url,
      content::WebContents* new_web_contents) override {}

  const GURL& tab_contents_url() const { return tab_contents_url_; }

  const GURL& last_search_url() const { return last_search_url_; }

  int navigate_in_tab_contents_times_called() const {
    return navigate_in_tab_contents_times_called_;
  }

 private:
  GURL tab_contents_url_;
  GURL last_search_url_;
  int navigate_in_tab_contents_times_called_ = 0;
};

}  // namespace

namespace test {

class SideSearchSideContentsHelperTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kSideSearch}, {});
    side_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SideSearchSideContentsHelper::CreateForWebContents(side_contents());
    helper()->SetDelegate(&delegate_);
    // Basic configuration for testing that will always show the side panel and
    // allows navigations to URLs matching `kSearchMatchUrl` to proceed
    // within the side panel.
    auto* config = SideSearchConfig::Get(&profile_);
    config->SetShouldNavigateInSidePanelCallback(base::BindRepeating(
        [&](const GURL& url) { return url == kSearchMatchUrl; }));
    config->SetCanShowSidePanelForURLCallback(
        base::BindRepeating([](const GURL& url) { return true; }));
    config->SetGenerateSideSearchURLCallback(
        base::BindRepeating([](const GURL& url) { return url; }));
    Test::SetUp();
  }

 protected:
  void LoadURL(const char* url) {
    const int initial_tab_navigation_count =
        delegate_.navigate_in_tab_contents_times_called();
    side_contents()->GetController().LoadURL(GURL(url), content::Referrer(),
                                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                             std::string());
    const int final_tab_navigation_count =
        delegate_.navigate_in_tab_contents_times_called();

    // If NavigateInTabContents() has been called this indicates that the side
    // contents' navigation has been cancelled by the throttle. Avoid committing
    // the side contents' navigation in this case as it will trigger a DCHECK().
    if (side_contents()->GetController().GetPendingEntry() &&
        initial_tab_navigation_count == final_tab_navigation_count) {
      content::WebContentsTester::For(side_contents())
          ->CommitPendingNavigation();
    }
  }

  content::NavigationEntry* GetLastCommittedSideContentsEntry() {
    return side_contents()->GetController().GetLastCommittedEntry();
  }

  const MockSideContentsDelegate& delegate() { return delegate_; }

  content::WebContents* side_contents() { return side_contents_.get(); }

  SideSearchSideContentsHelper* helper() {
    return SideSearchSideContentsHelper::FromWebContents(side_contents());
  }

  void ResetSideContents() { side_contents_.reset(); }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> side_contents_;
  MockSideContentsDelegate delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SideSearchSideContentsHelperTest,
       RedirectionConfigNavigatesSideContents) {
  LoadURL(kSearchMatchUrl);
  EXPECT_EQ(GURL(kSearchMatchUrl),
            GetLastCommittedSideContentsEntry()->GetURL());
  EXPECT_EQ(GURL(kSearchMatchUrl), delegate().last_search_url());
  EXPECT_TRUE(delegate().tab_contents_url().is_empty());
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.Navigation",
      SideSearchNavigationType::kNavigationCommittedWithinSideSearch, 1);
}

TEST_F(SideSearchSideContentsHelperTest,
       RedirectionConfigNavigatesTabContents) {
  LoadURL(kNonMatchUrl);
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());
  EXPECT_TRUE(delegate().last_search_url().is_empty());
  EXPECT_EQ(GURL(kNonMatchUrl), delegate().tab_contents_url());
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.Navigation", SideSearchNavigationType::kRedirectionToTab, 1);
}

TEST_F(SideSearchSideContentsHelperTest, EmitsPerJourneyMetrics) {
  // Ensure redirected navigations correctly log navigations
  LoadURL(kNonMatchUrl);
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());
  EXPECT_TRUE(delegate().last_search_url().is_empty());
  EXPECT_EQ(GURL(kNonMatchUrl), delegate().tab_contents_url());

  // Metrics should not be emitted until the side contents is destroyed.
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.RedirectionToTabCountPerJourney2", 1, 0);

  // A matching navigation will be allowed to proceed
  LoadURL(kSearchMatchUrl);
  EXPECT_EQ(GURL(kSearchMatchUrl),
            GetLastCommittedSideContentsEntry()->GetURL());
  EXPECT_EQ(GURL(kSearchMatchUrl), delegate().last_search_url());
  EXPECT_FALSE(delegate().tab_contents_url().is_empty());

  // Metrics should not be emitted until the side contents is destroyed.
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.NavigationCommittedWithinSideSearchCountPerJourney2", 1, 0);

  ResetSideContents();
  // Deleting the side contents should emit the search journey metris.
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.RedirectionToTabCountPerJourney2", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.NavigationCommittedWithinSideSearchCountPerJourney2", 1, 1);
}

TEST_F(SideSearchSideContentsHelperTest, EmitsPerJourneyMetricsAutotriggered) {
  // Set the auto-triggered flag to true.
  helper()->set_auto_triggered(true);

  // Ensure redirected navigations correctly log navigations
  LoadURL(kNonMatchUrl);
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());
  EXPECT_TRUE(delegate().last_search_url().is_empty());
  EXPECT_EQ(GURL(kNonMatchUrl), delegate().tab_contents_url());

  // Metrics should not be emitted until the side contents is destroyed.
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.RedirectionToTabCountPerJourney2", 1, 0);
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.AutoTrigger.RedirectionToTabCountPerJourney2", 1, 0);

  // A matching navigation will be allowed to proceed
  LoadURL(kSearchMatchUrl);
  EXPECT_EQ(GURL(kSearchMatchUrl),
            GetLastCommittedSideContentsEntry()->GetURL());
  EXPECT_EQ(GURL(kSearchMatchUrl), delegate().last_search_url());
  EXPECT_FALSE(delegate().tab_contents_url().is_empty());

  // Metrics should not be emitted until the side contents is destroyed.
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.NavigationCommittedWithinSideSearchCountPerJourney2", 1, 0);
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.AutoTrigger."
      "NavigationCommittedWithinSideSearchCountPerJourney2",
      1, 0);

  ResetSideContents();
  // Deleting the side contents should emit the search journey metris.
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.RedirectionToTabCountPerJourney2", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.AutoTrigger.RedirectionToTabCountPerJourney", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.NavigationCommittedWithinSideSearchCountPerJourney2", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.AutoTrigger."
      "NavigationCommittedWithinSideSearchCountPerJourney",
      1, 1);
}

TEST_F(SideSearchSideContentsHelperTest, EmitsPerJourneyMetricsFromMenuOption) {
  // Set helper created from menu option.
  helper()->set_is_created_from_menu_option(true);

  // Ensure redirected navigations correctly log navigations
  LoadURL(kNonMatchUrl);
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());
  EXPECT_TRUE(delegate().last_search_url().is_empty());
  EXPECT_EQ(GURL(kNonMatchUrl), delegate().tab_contents_url());

  // Metrics should not be emitted until the side contents is destroyed.
  histogram_tester_.ExpectTotalCount(
      "SideSearch.RedirectionToTabCountPerJourneyFromMenuOption", 0);

  // A matching navigation will be allowed to proceed
  LoadURL(kSearchMatchUrl);
  EXPECT_EQ(GURL(kSearchMatchUrl),
            GetLastCommittedSideContentsEntry()->GetURL());
  EXPECT_EQ(GURL(kSearchMatchUrl), delegate().last_search_url());
  EXPECT_FALSE(delegate().tab_contents_url().is_empty());

  // Metrics should not be emitted until the side contents is destroyed.
  histogram_tester_.ExpectTotalCount(
      "SideSearch."
      "NavigationCommittedWithinSideSearchCountPerJourneyFromMenuOption",
      0);

  ResetSideContents();
  // Deleting the side contents should emit the search journey metris.
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.RedirectionToTabCountPerJourneyFromMenuOption", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "SideSearch."
      "NavigationCommittedWithinSideSearchCountPerJourneyFromMenuOption",
      1, 1);
}

}  // namespace test
