// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/accuracy_tips/accuracy_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_info/chrome_accuracy_tip_ui.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/test/widget_test_api.h"

namespace {
using accuracy_tips::AccuracyTipStatus;
using accuracy_tips::AccuracyTipUI;

bool IsUIShowing() {
  return PageInfoBubbleViewBase::BUBBLE_ACCURACY_TIP ==
         PageInfoBubbleViewBase::GetShownBubbleType();
}

}  // namespace

class AccuracyTipBubbleViewBrowserTest : public InProcessBrowserTest {
 protected:
  GURL GetUrl(const std::string& host) {
    return embedded_test_server()->GetURL(host, "/title1.html");
  }

  void SetUp() override {
    feature_list_.InitAndEnableFeature(safe_browsing::kAccuracyTipsFeature);

    // Disable "close on deactivation" since there seems to be an issue with
    // windows losing focus during tests.
    views::DisableActivationChangeHandlingForTests();

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    AccuracyServiceFactory::GetForProfile(browser()->profile())
        ->SetSampleUrlForTesting(GetUrl("badurl.com"));
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, NoShowOnRegularUrl) {
  ui_test_utils::NavigateToURL(browser(), GetUrl("example.com"));
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample("Privacy.AccuracyTip.PageStatus",
                                         AccuracyTipStatus::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, ShowOnBadUrl) {
  ui_test_utils::NavigateToURL(browser(), GetUrl("badurl.com"));
  EXPECT_TRUE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.PageStatus", AccuracyTipStatus::kShowAccuracyTip, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, PressIgnoreButton) {
  ui_test_utils::NavigateToURL(browser(), GetUrl("badurl.com"));
  EXPECT_TRUE(IsUIShowing());

  auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->CancelDialog();
  waiter.Wait();
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipUI::Interaction::kIgnorePressed, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, DisappearOnNavigate) {
  ui_test_utils::NavigateToURL(browser(), GetUrl("badurl.com"));
  EXPECT_TRUE(IsUIShowing());

  // Tip disappears when navigating somewhere else.
  auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  ui_test_utils::NavigateToURL(browser(), GetUrl("example.com"));
  waiter.Wait();
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipUI::Interaction::kNoAction, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, OpenLearnMoreLink) {
  ui_test_utils::NavigateToURL(browser(), GetUrl("badurl.com"));
  EXPECT_TRUE(IsUIShowing());

  // Click "learn more" and expect help center to open.
  auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  content::WebContentsAddedObserver new_tab_observer;
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->AcceptDialog();
  EXPECT_EQ(GURL(chrome::kSafetyTipHelpCenterURL),
            new_tab_observer.GetWebContents()->GetURL());
  waiter.Wait();
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipUI::Interaction::kLearnMorePressed, 1);
}

// Render test for accuracy tip ui.
class AccuracyTipBubbleViewDialogBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowAccuracyTipDialog(browser()->tab_strip_model()->GetActiveWebContents(),
                          accuracy_tips::AccuracyTipStatus::kShowAccuracyTip,
                          base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
