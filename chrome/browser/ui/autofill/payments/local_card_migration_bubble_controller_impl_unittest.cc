// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/local_card_migration_metrics.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace autofill {
namespace {

class TestLocalCardMigrationBubbleControllerImpl
    : public LocalCardMigrationBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestLocalCardMigrationBubbleControllerImpl>(
            web_contents));
  }

  explicit TestLocalCardMigrationBubbleControllerImpl(
      content::WebContents* web_contents)
      : LocalCardMigrationBubbleControllerImpl(web_contents) {}

  void SimulateNavigation() {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    content::MockNavigationHandle navigation_handle(GURL(), rfh);
    navigation_handle.set_has_committed(true);
    DidFinishNavigation(&navigation_handle);
  }
};

class LocalCardMigrationBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  LocalCardMigrationBubbleControllerImplTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  LocalCardMigrationBubbleControllerImplTest(
      const LocalCardMigrationBubbleControllerImplTest&) = delete;
  LocalCardMigrationBubbleControllerImplTest& operator=(
      const LocalCardMigrationBubbleControllerImplTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestLocalCardMigrationBubbleControllerImpl::CreateForTesting(web_contents);
  }

 protected:
  void ShowBubble() {
    controller()->ShowBubble(base::BindOnce(&LocalCardMigrationCallback));
  }

  void CloseBubble(PaymentsBubbleClosedReason closed_reason =
                       PaymentsBubbleClosedReason::kNotInteracted) {
    controller()->OnBubbleClosed(closed_reason);
  }

  void CloseAndReshowBubble() {
    CloseBubble();
    controller()->ReshowBubble();
  }

  TestLocalCardMigrationBubbleControllerImpl* controller() {
    return static_cast<TestLocalCardMigrationBubbleControllerImpl*>(
        TestLocalCardMigrationBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

 private:
  static void LocalCardMigrationCallback() {}
};

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Metrics_FirstShow_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, Metrics_Reshows_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.Reshows"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       OnlyOneActiveBubble_Repeated) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  ShowBubble();
  ShowBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

// Ensures the bubble should still stick around even if the time since bubble
// showing is longer than kCardBubbleSurviveNavigationTime (5 seconds) when the
// feature is enabled.
TEST_F(LocalCardMigrationBubbleControllerImplTest,
       StickyBubble_ShouldNotDismissUponNavigation) {
  ShowBubble();
  base::HistogramTester histogram_tester;
  task_environment()->FastForwardBy(base::Seconds(10));
  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
  EXPECT_NE(nullptr, controller()->local_card_migration_bubble_view());
}

// Test class to ensure the local card migration bubble result is logged
// correctly.
TEST_F(LocalCardMigrationBubbleControllerImplTest, FirstShow_BubbleAccepted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, FirstShow_BubbleClosed) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       FirstShow_BubbleNotInteracted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, FirstShow_BubbleLostFocus) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, FirstShow_Unknown) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kUnknown);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_RESULT_UNKNOWN, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, Reshows_BubbleAccepted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.Reshows",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, Reshows_BubbleClosed) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.Reshows",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest,
       Reshows_BubbleNotInteracted) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.Reshows",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, Reshows_BubbleLostFocus) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.Reshows",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS, 1);
}

TEST_F(LocalCardMigrationBubbleControllerImplTest, Reshows_Unknown) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  CloseAndReshowBubble();
  CloseBubble(PaymentsBubbleClosedReason::kUnknown);

  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.FirstShow",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleResult.Reshows",
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_RESULT_UNKNOWN, 1);
}

}  // namespace
}  // namespace autofill
