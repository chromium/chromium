// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {
namespace {

constexpr char kShowAction[] = "LensSidePanel.Show";
constexpr char kHideAction[] = "LensSidePanel.Hide";
constexpr char kHideChromeSidePanelAction[] =
    "LensSidePanel.HideChromeSidePanel";
constexpr char kLensQueryWhileShowingAction[] =
    "LensSidePanel.LensQueryWhileShowing";
constexpr char kCloseButtonClickAction[] = "LensSidePanel.CloseButtonClick";

class LensSidePanelControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeaturesAndParameters(
        {{features::kLensStandalone, {{"enable-side-panel", "true"}}}}, {});
    TestWithBrowserView::SetUp();
    // Create the lens side panel controller in BrowserView.
    browser_view()->CreateLensSidePanelController();

    // Create an active web contents.
    AddTab(browser_view()->browser(), GURL("about:blank"));
    controller_ = browser_view()->lens_side_panel_controller();
  }

 protected:
  raw_ptr<LensSidePanelController> controller_;
};

TEST_F(LensSidePanelControllerTest, OpenWithURLShowsLensSidePanel) {
  base::UserActionTester user_action_tester;

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kShowAction));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kHideChromeSidePanelAction));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kLensQueryWhileShowingAction));
}

TEST_F(LensSidePanelControllerTest, OpenWithURLRecordsMultipleLensQueries) {
  base::UserActionTester user_action_tester;

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://bar.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kShowAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLensQueryWhileShowingAction));
}

TEST_F(LensSidePanelControllerTest, OpenWithURLHidesChromeSidePanel) {
  base::UserActionTester user_action_tester;
  browser_view()->unified_side_panel()->SetVisible(true);

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kShowAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kHideChromeSidePanelAction));
}

TEST_F(LensSidePanelControllerTest, DISABLED_CloseAfterOpenHidesLensSidePanel) {
  base::UserActionTester user_action_tester;
  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  controller_->Close();

  EXPECT_FALSE(browser_view()->lens_side_panel_controller());
  EXPECT_FALSE(browser_view()->lens_side_panel()->GetVisible());
  // default side panel has a single empty view as child
  EXPECT_EQ((unsigned long)1,
            browser_view()->lens_side_panel()->children().size());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kHideAction));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kCloseButtonClickAction));
}

TEST_F(LensSidePanelControllerTest, ReOpensAndCloses) {
  base::UserActionTester user_action_tester;

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  EXPECT_TRUE(browser_view()->lens_side_panel_controller());
  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());

  // Closing the controller should hide side panel and delete controller
  // pointer.
  controller_->Close();
  EXPECT_FALSE(browser_view()->lens_side_panel_controller());
  EXPECT_FALSE(browser_view()->lens_side_panel()->GetVisible());

  // Creating a new controller in browser view should fix pointer, but side
  // panel is still not visible.
  browser_view()->CreateLensSidePanelController();
  controller_ = browser_view()->lens_side_panel_controller();
  EXPECT_TRUE(browser_view()->lens_side_panel_controller());
  EXPECT_FALSE(browser_view()->lens_side_panel()->GetVisible());

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://bar.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  EXPECT_TRUE(browser_view()->lens_side_panel_controller());
  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());
  controller_->Close();

  EXPECT_FALSE(browser_view()->lens_side_panel_controller());
  EXPECT_FALSE(browser_view()->lens_side_panel()->GetVisible());
  EXPECT_EQ(2, user_action_tester.GetActionCount(kShowAction));
  EXPECT_EQ(2, user_action_tester.GetActionCount(kHideAction));
}

TEST_F(LensSidePanelControllerTest,
       LoadResultsInNewTabDoesNotHideLensSidePanel) {
  base::UserActionTester user_action_tester;

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  // Because we now use the last committed URL of the side panel's webview to
  // build the new tab link, we no longer hide the side panel every time the new
  // tab button is clicked. We only close it when the new tab button was able to
  // create a valid URL.
  controller_->LoadResultsInNewTab();

  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());
  EXPECT_EQ(0, user_action_tester.GetActionCount(kHideAction));
}

TEST_F(LensSidePanelControllerTest,
       OpenAndCloseLensSidePanelDeletesController) {
  base::UserActionTester user_action_tester;

  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  EXPECT_TRUE(browser_view()->lens_side_panel_controller());
  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());

  // Closing the controller should hide side panel and delete controller
  // pointer.
  controller_->Close();
  EXPECT_FALSE(browser_view()->lens_side_panel_controller());
  EXPECT_FALSE(browser_view()->lens_side_panel()->GetVisible());

  // Creating a new controller in browser view should fix pointer, but side
  // panel is still not visible.
  browser_view()->CreateLensSidePanelController();
  controller_ = browser_view()->lens_side_panel_controller();
  EXPECT_TRUE(browser_view()->lens_side_panel_controller());
  EXPECT_FALSE(browser_view()->lens_side_panel()->GetVisible());

  // Reopening the side panel with URL should now make the side panel visible
  // again.
  controller_->OpenWithURL(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  EXPECT_TRUE(browser_view()->lens_side_panel_controller());
  EXPECT_TRUE(browser_view()->lens_side_panel()->GetVisible());
  EXPECT_EQ(2, user_action_tester.GetActionCount(kShowAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kHideAction));
}

}  // namespace
}  // namespace lens
