// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include <memory>

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

constexpr char kGoogleSearchURL1[] = "https://www.google.com/search?q=test1";
constexpr char kGoogleSearchURL2[] = "https://www.google.com/search?q=test2";
constexpr char kGoogleSearchHomePageURL[] = "https://www.google.com";
constexpr char kNonGoogleURL[] = "https://www.test.com";

// Tests to see if `navigated_url` matches `stored_url`. We cannot directly
// compare as the "sidesearch=1" query param may have been appended to the
// `stored_url` to ensure Google serves the side search SRP to the chrome
// client.
bool DoesURLMatch(const char* navigated_url, const GURL& stored_url) {
  return stored_url.spec().find(GURL(navigated_url).spec()) !=
         std::string::npos;
}

bool DoesURLMatch(const char* navigated_url,
                  const absl::optional<GURL>& stored_url) {
  return stored_url && DoesURLMatch(navigated_url, stored_url.value());
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
    SideSearchConfig::Get(&profile_)->set_is_side_panel_srp_available(true);
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SideSearchTabContentsHelperTest, LastSearchURLUpdatesCorrectly) {
  // When a tab is first opened there should be no last encountered Google SRP.
  EXPECT_FALSE(helper()->last_search_url().has_value());
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());

  // Navigating to a Google SRP should update the `last_search_url`.
  LoadURL(kGoogleSearchURL1);
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  // Navigating to a non-Google SRP URL should not change the `last_search_url`.
  LoadURL(kNonGoogleURL);
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  // Navigating again to a Google SRP should update the `last_search_url`.
  LoadURL(kGoogleSearchURL2);
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  // Going backwards to the non-Google SRP URL should not update the last search
  // url.
  GoBack();
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  // Going back to the original Google SRP should update the `last_search_url`
  // to that Google SRP URL.
  GoBack();
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  // Going forward to the non-Google URL shouldn't change the `last_search_url`.
  GoForward();
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  // Going forward to the Google SRP URL should update the `last_search_url` to
  // that Google SRP URL.
  GoForward();
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2,
                           GetLastCommittedSideContentsEntry()->GetURL()));
}

TEST_F(SideSearchTabContentsHelperTest, LastSearchURLIgnoresGoogleHomePage) {
  EXPECT_FALSE(helper()->last_search_url().has_value());

  LoadURL(kGoogleSearchURL1);
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  // Navigating to the Google home page should not update the `last_search_url`.
  LoadURL(kGoogleSearchHomePageURL);
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL1,
                           GetLastCommittedSideContentsEntry()->GetURL()));

  LoadURL(kGoogleSearchURL2);
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2, helper()->last_search_url()));
  EXPECT_TRUE(DoesURLMatch(kGoogleSearchURL2,
                           GetLastCommittedSideContentsEntry()->GetURL()));
}

TEST_F(SideSearchTabContentsHelperTest, IndicatesWhenSidePanelShouldBeShown) {
  // With no initial navigation the side panel should not be showing.
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // If no previous Google SRP has been seen for this tab contents the side
  // panel should not show.
  LoadURL(kNonGoogleURL);
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // The side panel should not be visible on Google SRP pages.
  LoadURL(kGoogleSearchURL1);
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // If a Google SRP has previously been navigated to it can appear in the side
  // panel if on a non-Google page.
  LoadURL(kNonGoogleURL);
  EXPECT_TRUE(helper()->CanShowSidePanelForCommittedNavigation());

  // The side panel should not appear when on the Google home page.
  LoadURL(kGoogleSearchHomePageURL);
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());
}

TEST_F(SideSearchTabContentsHelperTest, ClearsSidePanelContentsWhenAsked) {
  EXPECT_NE(nullptr, helper()->side_panel_contents_for_testing());
  helper()->ClearSidePanelContents();
  EXPECT_EQ(nullptr, helper()->side_panel_contents_for_testing());
}

TEST_F(SideSearchTabContentsHelperTest,
       SidePanelProcessGoneResetsSideContents) {
  EXPECT_NE(nullptr, helper()->side_panel_contents_for_testing());
  helper()->SidePanelProcessGone();
  EXPECT_EQ(nullptr, helper()->side_panel_contents_for_testing());
}

}  // namespace test
