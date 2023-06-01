// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_stats_tab_helper.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTestUrl1[] = "https://www.test-url-1.com/";
constexpr char kTestUrl2[] = "https://www.test-url-2.com/";

}  // namespace

namespace test {

class BookmarkStatsTabHelperTest : public ::testing::Test {
 public:
  // ::testing::Test:
  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    Test::SetUp();
  }

 protected:
  void LoadUrl(const char* url) {
    web_contents()->GetController().LoadURL(GURL(url), content::Referrer(),
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());
  }

  void CommitPendingNavigation() {
    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
  }

  BookmarkStatsTabHelper* GetHelper() {
    BookmarkStatsTabHelper::CreateForWebContents(web_contents_.get());
    return BookmarkStatsTabHelper::FromWebContents(web_contents_.get());
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::HistogramTester histogram_tester_;
};

TEST_F(BookmarkStatsTabHelperTest, LaunchActionAddedWithPendingEntry) {
  // The launch action should start unset.
  BookmarkStatsTabHelper* helper = GetHelper();
  EXPECT_FALSE(helper->launch_action_for_testing().has_value());
  EXPECT_FALSE(helper->tab_disposition_for_testing().has_value());

  // Attempting to set the launch action without a pending navigation having
  // been set should result in a no-op.
  helper->SetLaunchAction(
      {BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()},
      WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(helper->launch_action_for_testing().has_value());
  EXPECT_FALSE(helper->tab_disposition_for_testing().has_value());

  // Begin a navigation and attempt to set the launch action. This should now
  // be reflected in the helper.
  LoadUrl(kTestUrl1);
  helper->SetLaunchAction(
      {BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()},
      WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(helper->launch_action_for_testing().has_value());
  EXPECT_TRUE(helper->tab_disposition_for_testing().has_value());

  // After the navigation completes the launch action data should remain.
  CommitPendingNavigation();
  EXPECT_TRUE(helper->launch_action_for_testing().has_value());
  EXPECT_TRUE(helper->tab_disposition_for_testing().has_value());

  // A following navigation should clear the launch action data.
  LoadUrl(kTestUrl2);
  CommitPendingNavigation();
  EXPECT_FALSE(helper->launch_action_for_testing().has_value());
  EXPECT_FALSE(helper->tab_disposition_for_testing().has_value());
}

TEST_F(BookmarkStatsTabHelperTest, EmitsNonEmptyPaintMetrics) {
  // The launch action should start unset.
  BookmarkStatsTabHelper* helper = GetHelper();
  EXPECT_FALSE(helper->launch_action_for_testing().has_value());
  EXPECT_FALSE(helper->tab_disposition_for_testing().has_value());
  histogram_tester().ExpectTotalCount(
      "Bookmarks.AttachedBar.CurrentTab.TimeToFirstVisuallyNonEmptyPaint", 0);

  // Begin a navigation and attempt to set the launch action. This should now
  // be reflected in the helper.
  LoadUrl(kTestUrl1);
  helper->SetLaunchAction(
      {BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()},
      WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(helper->launch_action_for_testing().has_value());
  EXPECT_TRUE(helper->tab_disposition_for_testing().has_value());

  // Simulate a non-empty paint event. The non-empty paint metric should be
  // emitted.
  helper->DidFirstVisuallyNonEmptyPaint();
  histogram_tester().ExpectTotalCount(
      "Bookmarks.AttachedBar.CurrentTab.TimeToFirstVisuallyNonEmptyPaint", 1);

  // A following navigation should clear the launch action data.
  LoadUrl(kTestUrl2);
  CommitPendingNavigation();
  EXPECT_FALSE(helper->launch_action_for_testing().has_value());
  EXPECT_FALSE(helper->tab_disposition_for_testing().has_value());

  // Simulate a non-empty paint event. No metrics should have been emitted as
  // the launch action should have been reset.
  helper->DidFirstVisuallyNonEmptyPaint();
  histogram_tester().ExpectTotalCount(
      "Bookmarks.AttachedBar.CurrentTab.TimeToFirstVisuallyNonEmptyPaint", 1);
}

}  // namespace test
