// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class DownloadBubbleInteractiveUiTest : public DownloadTestBase,
                                        public InteractiveBrowserTestApi {
 public:
  DownloadBubbleInteractiveUiTest() {
    test_features_.InitAndEnableFeatures(
        {feature_engagement::kIPHDownloadToolbarButtonFeature,
         safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2},
        {});
  }

  DownloadToolbarButtonView* download_toolbar_button() {
    BrowserView* const browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->download_button();
  }

  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
    private_test_impl().DoTestSetUp();
    SetContextWidget(
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());

    // Disable the auto-close timer and animation to prevent flakiness.
    download_toolbar_button()->DisableAutoCloseTimerForTesting();
    download_toolbar_button()->DisableDownloadStartedAnimationForTesting();

    ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetFeaturePromoController()));
  }

  void TearDownOnMainThread() override {
    SetContextWidget(nullptr);
    private_test_impl().DoTestTearDown();
    DownloadTestBase::TearDownOnMainThread();
  }

  auto DownloadBubbleIsShowingDetails(bool showing) {
    return base::BindLambdaForTesting([&, showing = showing]() {
      return showing == download_toolbar_button()->IsShowingDetails();
    });
  }

  auto DownloadBubblePromoIsActive(bool active) {
    return base::BindLambdaForTesting([&, active = active]() {
      return active ==
             BrowserView::GetBrowserViewForBrowser(browser())
                 ->GetFeaturePromoController()
                 ->IsPromoActive(
                     feature_engagement::kIPHDownloadToolbarButtonFeature);
    });
  }

  auto ChangeButtonVisibility(bool visible) {
    return base::BindLambdaForTesting([&, visible = visible]() {
      if (visible) {
        download_toolbar_button()->Show();
      } else {
        download_toolbar_button()->Hide();
      }
    });
  }

  auto ChangeBubbleVisibility(bool visible) {
    return base::BindLambdaForTesting([&, visible = visible]() {
      if (visible) {
        download_toolbar_button()->ShowDetails();
      } else {
        download_toolbar_button()->HideDetails();
      }
    });
  }

  auto DownloadTestFile() {
    GURL url = embedded_test_server()->GetURL(
        base::StrCat({"/", DownloadTestBase::kDownloadTest1Path}));
    return base::BindLambdaForTesting(
        [this, url]() { DownloadAndWait(browser(), url); });
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       ToolbarIconAndBubbleDetailsShownAfterDownload) {
  RunTestSequence(Do(DownloadTestFile()),
                  WaitForShow(kDownloadToolbarButtonElementId),
                  Check(DownloadBubbleIsShowingDetails(true)),
                  // Hide the bubble so it's not showing while tearing down the
                  // test browser (which causes a crash on Mac).
                  Do(ChangeBubbleVisibility(false)));
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       DownloadBubbleInteractedWith_NoIPHShown) {
  RunTestSequence(Do(ChangeButtonVisibility(true)),
                  WaitForShow(kDownloadToolbarButtonElementId),
                  Check(DownloadBubbleIsShowingDetails(false)),
                  // Press the button to register an interaction (which should
                  // suppress the IPH) which opens the main view.
                  PressButton(kDownloadToolbarButtonElementId),
                  // Close the main view.
                  Do(ChangeBubbleVisibility(false)),
                  // Now download a file to show the partial view.
                  Do(DownloadTestFile()),
                  Check(DownloadBubbleIsShowingDetails(true)),
                  // Hide the partial view. No IPH is shown.
                  Do(ChangeBubbleVisibility(false)),
                  Check(DownloadBubbleIsShowingDetails(false)),
                  Check(DownloadBubblePromoIsActive(false)));
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       DownloadBubbleShownAfterDownload_IPHShown) {
  RunTestSequence(Do(DownloadTestFile()),
                  WaitForShow(kDownloadToolbarButtonElementId),
                  Check(DownloadBubbleIsShowingDetails(true)),
                  // Hide the partial view. The IPH should be shown.
                  Do(ChangeBubbleVisibility(false)),
                  Check(DownloadBubbleIsShowingDetails(false)),
                  Check(DownloadBubblePromoIsActive(true)));
}

}  // namespace
