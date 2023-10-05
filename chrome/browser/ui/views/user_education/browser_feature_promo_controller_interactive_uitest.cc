// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

using user_education::FeaturePromoCloseReason;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoSpecification;
using CloseReason = user_education::FeaturePromoStorageService::CloseReason;

namespace {
BASE_FEATURE(kToastTestFeature,
             "ToastTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionTestFeature,
             "CustomActionTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLegalNoticeTestFeature,
             "LegalNoticeTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

class BrowserFeaturePromoControllerUiTest : public InteractiveBrowserTest {
 public:
  BrowserFeaturePromoControllerUiTest() {
    subscription_ = BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(RegisterMockTracker));
  }
  ~BrowserFeaturePromoControllerUiTest() override = default;

  BrowserFeaturePromoControllerUiTest(
      const BrowserFeaturePromoControllerUiTest&) = delete;
  BrowserFeaturePromoControllerUiTest& operator=(
      const BrowserFeaturePromoControllerUiTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                browser()->profile()));
    ASSERT_TRUE(mock_tracker_);

    // Allow an unlimited number of calls to WouldTriggerHelpUI().
    EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI)
        .WillRepeatedly(Return(true));

    promo_controller_ = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());

    // Register test features.
    registry()->RegisterFeature(
        user_education::FeaturePromoSpecification::CreateForToastPromo(
            kToastTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            FeaturePromoSpecification::AcceleratorInfo()));
    registry()->RegisterFeature(
        user_education::FeaturePromoSpecification::CreateForCustomAction(
            kCustomActionTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            base::DoNothing()));

    auto notice =
        user_education::FeaturePromoSpecification::CreateForCustomAction(
            kLegalNoticeTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            base::DoNothing());
    notice.set_promo_subtype_for_testing(
        user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    registry()->RegisterFeature(std::move(notice));
  }

  // Verifies that `CanShowPromo()` returns `expected_result`.
  auto QueryIPH(const base::Feature& iph_feature,
                user_education::FeaturePromoResult expected_result) {
    std::ostringstream oss;
    oss << "QueryIPH(" << iph_feature.name << ", " << expected_result << ")";
    return CheckResult(
        [this, &iph_feature]() {
          return promo_controller_->CanShowPromo(iph_feature);
        },
        expected_result, oss.str());
  }

  // Tries to show tab groups IPH by meeting the trigger conditions. If
  // |should_show| is true it checks that it was shown. If false, it
  // checks that it was not shown.
  auto AttemptIPH(const base::Feature& iph_feature,
                  user_education::FeaturePromoResult expected_result,
                  base::OnceClosure on_close = base::DoNothing()) {
    return Check([this, &iph_feature, expected_result,
                  callback = std::move(on_close)]() mutable {
      if (expected_result) {
        EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(iph_feature)))
            .WillOnce(Return(true));
      } else {
        EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(iph_feature)))
            .Times(0);
      }

      user_education::FeaturePromoParams params(iph_feature);
      params.close_callback = std::move(callback);
      if (expected_result !=
          promo_controller_->MaybeShowPromo(std::move(params))) {
        LOG(ERROR) << "MaybeShowPromo() didn't return expected result.";
        return false;
      }
      if (expected_result != promo_controller_->IsPromoActive(iph_feature)) {
        LOG(ERROR) << "IsPromoActive() didn't return expected result.";
        return false;
      }

      // If shown, Tracker::Dismissed should be called eventually.
      if (expected_result) {
        EXPECT_CALL(*mock_tracker_, Dismissed(Ref(iph_feature)));
      }
      return true;
    });
  }

  auto TriggerNonDefaultButton() {
    return Steps(
        PressButton(
            user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  auto DismissIPH() {
    return Steps(
        PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  auto CancelIPH() {
    return Steps(
        PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  auto UseFeature(const base::Feature& iph_feature) {
    return Steps(Do(base::BindLambdaForTesting([this, &iph_feature]() {
      EXPECT_TRUE(promo_controller_->IsPromoActive(iph_feature));
      promo_controller_->EndPromo(iph_feature,
                                  FeaturePromoCloseReason::kFeatureEngaged);

      EXPECT_FALSE(promo_controller_->IsPromoActive(iph_feature));
    })));
  }

  auto AbortIPH(const base::Feature& iph_feature) {
    return Steps(Do(base::BindLambdaForTesting([this, &iph_feature]() {
      EXPECT_TRUE(promo_controller_->IsPromoActive(iph_feature));
      promo_controller_->EndPromo(iph_feature,
                                  FeaturePromoCloseReason::kAbortPromo);

      EXPECT_FALSE(promo_controller_->IsPromoActive(iph_feature));
    })));
  }

  auto CheckMetrics(const base::Feature& iph_feature,
                    int dismiss_count,
                    int snooze_count,
                    int abort_count,
                    int feature_engaged_count,
                    int custom_action_count) {
    return Check(
        base::BindLambdaForTesting([this, &iph_feature, dismiss_count,
                                    snooze_count, abort_count,
                                    feature_engaged_count,
                                    custom_action_count]() {
          EXPECT_EQ(dismiss_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Dismiss.")
                            .append(iph_feature.name)));
          EXPECT_EQ(snooze_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Snooze.")
                            .append(iph_feature.name)));
          EXPECT_EQ(abort_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Abort.")
                            .append(iph_feature.name)));
          EXPECT_EQ(
              feature_engaged_count,
              user_action_tester_.GetActionCount(
                  std::string("UserEducation.MessageAction.FeatureEngaged.")
                      .append(iph_feature.name)));
          EXPECT_EQ(custom_action_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Action.")
                            .append(iph_feature.name)));

          std::string action_name = "UserEducation.MessageAction.";
          action_name.append(iph_feature.name);

          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(CloseReason::kDismiss),
              dismiss_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(CloseReason::kSnooze),
              snooze_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(CloseReason::kAbortPromo),
              abort_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(CloseReason::kFeatureEngaged),
              feature_engaged_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(CloseReason::kAction),
              custom_action_count);

          return !testing::Test::HasNonfatalFailure();
        }),
        "Metrics");
  }

  user_education::FeaturePromoController* promo_controller() const {
    return promo_controller_;
  }

 private:
  UserEducationService* factory() {
    return UserEducationServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  FeaturePromoRegistry* registry() {
    return &factory()->feature_promo_registry();
  }

  static void RegisterMockTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(CreateMockTracker));
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* context) {
    auto mock_tracker =
        std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

    // Allow any other IPH to call, but don't ever show them.
    EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    return mock_tracker;
  }

  raw_ptr<NiceMock<feature_engagement::test::MockTracker>,
          AcrossTasksDanglingUntriaged>
      mock_tracker_;
  raw_ptr<BrowserFeaturePromoController, AcrossTasksDanglingUntriaged>
      promo_controller_;
  base::CallbackListSubscription subscription_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CanShowPromoForElement) {
  auto widget = std::make_unique<views::Widget>();

  auto can_show_promo = [this](ui::TrackedElement* anchor) {
    return static_cast<BrowserFeaturePromoController*>(
               browser()->window()->GetFeaturePromoController())
        ->CanShowPromoForElement(anchor);
  };

  RunTestSequence(
      // Verify that at first, we can show the promo on the browser.
      CheckElement(kToolbarAppMenuButtonElementId, can_show_promo, true),
      // Start observing widget focus, and create the widget.
      ObserveState(views::test::kCurrentWidgetFocus),
      // Create a second widget and give it focus. We can't guarantee that we
      // can deactivate unless there is a second window, because of how some
      // platforms handle focus.
      WithView(kBrowserViewElementId,
               [&widget](BrowserView* browser_view) {
                 views::Widget::InitParams params(
                     views::Widget::InitParams::TYPE_WINDOW);
                 params.context = browser_view->GetWidget()->GetNativeWindow();
                 params.ownership =
                     views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
                 params.bounds = gfx::Rect(0, 0, 200, 200);
                 widget->Init(std::move(params));

                 // Doing this dance will make sure the necessary message gets
                 // sent to the window on all platforms we care about.
                 widget->Show();
                 browser_view->GetWidget()->Deactivate();
                 widget->Activate();
               }),
      // Wait for widget activation to move to the new widget.
      WaitForState(views::test::kCurrentWidgetFocus,
                   [&widget]() { return widget->GetNativeView(); }),
      // Verify that we can no longer show the promo, since the browser is not
      // the active window.
      CheckElement(kToolbarAppMenuButtonElementId, can_show_promo, false));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest, LogsAbortMetrics) {
  RunTestSequence(AttemptIPH(kToastTestFeature,
                             user_education::FeaturePromoResult::Success()),
                  AbortIPH(kToastTestFeature),
                  CheckMetrics(kToastTestFeature,
                               /*snooze_count*/ 0,
                               /*dismiss_count*/ 0,
                               /*abort_count*/ 1,
                               /*feature_engaged_count*/ 0,
                               /*custom_action_count*/ 0));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       LogsEngagedMetrics) {
  RunTestSequence(AttemptIPH(kToastTestFeature,
                             user_education::FeaturePromoResult::Success()),
                  UseFeature(kToastTestFeature),
                  CheckMetrics(kToastTestFeature,
                               /*snooze_count*/ 0,
                               /*dismiss_count*/ 0,
                               /*abort_count*/ 0,
                               /*feature_engaged_count*/ 1,
                               /*custom_action_count*/ 0));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       LogsCustomActionMetrics) {
  RunTestSequence(AttemptIPH(kCustomActionTestFeature,
                             user_education::FeaturePromoResult::Success()),
                  TriggerNonDefaultButton(),
                  CheckMetrics(kCustomActionTestFeature,
                               /*snooze_count*/ 0,
                               /*dismiss_count*/ 0,
                               /*abort_count*/ 0,
                               /*feature_engaged_count*/ 0,
                               /*custom_action_count*/ 1));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CanShowPromoReturnsExpectedValue) {
  RunTestSequence(QueryIPH(kToastTestFeature,
                           user_education::FeaturePromoResult::Success()),
                  AttemptIPH(kToastTestFeature,
                             user_education::FeaturePromoResult::Success()),
                  QueryIPH(kToastTestFeature,
                           user_education::FeaturePromoResult::kBlockedByPromo),
                  CancelIPH(),
                  QueryIPH(kToastTestFeature,
                           user_education::FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterCancel) {
  bool called = false;
  CloseReason close_reason = CloseReason::kAbortPromo;

  RunTestSequence(
      AttemptIPH(kCustomActionTestFeature,
                 user_education::FeaturePromoResult::Success(),
                 base::BindLambdaForTesting([this, &called, &close_reason]() {
                   called = true;
                   EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
                       kCustomActionTestFeature, &close_reason));
                 })),
      CancelIPH(), Check([&called]() { return called; }),
      CheckResult([&close_reason]() { return close_reason; },
                  CloseReason::kCancel));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterConfirm) {
  bool called = false;
  CloseReason close_reason = CloseReason::kAbortPromo;

  RunTestSequence(
      AttemptIPH(kCustomActionTestFeature,
                 user_education::FeaturePromoResult::Success(),
                 base::BindLambdaForTesting([this, &called, &close_reason]() {
                   called = true;
                   EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
                       kCustomActionTestFeature, &close_reason));
                 })),
      DismissIPH(), Check([&called]() { return called; }),
      CheckResult([&close_reason]() { return close_reason; },
                  CloseReason::kDismiss));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterCustomAction) {
  bool called = false;
  CloseReason close_reason = CloseReason::kAbortPromo;

  RunTestSequence(
      AttemptIPH(
          // Normal promos will defer writing close data until the promo is
          // fully ended.
          kLegalNoticeTestFeature,
          user_education::FeaturePromoResult::Success(),
          base::BindLambdaForTesting([this, &called, &close_reason]() {
            called = true;
            EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
                kLegalNoticeTestFeature, &close_reason));
          })),
      TriggerNonDefaultButton(), Check([&called]() { return called; }),
      CheckResult([&close_reason]() { return close_reason; },
                  CloseReason::kAction));
}
