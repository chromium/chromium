// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
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
using ::testing::Ref;
using ::testing::Return;

using user_education::EndFeaturePromoReason;
using user_education::FeaturePromoClosedReason;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoSpecification;

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

class BrowserFeaturePromoControllerUiTest : public InteractiveFeaturePromoTest {
 public:
  BrowserFeaturePromoControllerUiTest()
      : InteractiveFeaturePromoTest(UseMockTracker(),
                                    ClockMode::kUseDefaultClock) {}

  ~BrowserFeaturePromoControllerUiTest() override = default;

  BrowserFeaturePromoControllerUiTest(
      const BrowserFeaturePromoControllerUiTest&) = delete;
  BrowserFeaturePromoControllerUiTest& operator=(
      const BrowserFeaturePromoControllerUiTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();

    // Register test features.
    RegisterTestFeature(
        browser(),
        user_education::FeaturePromoSpecification::CreateForToastPromo(
            kToastTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            FeaturePromoSpecification::AcceleratorInfo()));
    RegisterTestFeature(
        browser(),
        user_education::FeaturePromoSpecification::CreateForCustomAction(
            kCustomActionTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            base::DoNothing()));

    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForCustomAction(
                kLegalNoticeTestFeature, kToolbarAppMenuButtonElementId,
                IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE,
                IDS_TUTORIAL_TAB_GROUP_COLLAPSE, base::DoNothing())
                .set_promo_subtype_for_testing(
                    user_education::FeaturePromoSpecification::PromoSubtype::
                        kLegalNotice)));
  }

  // Verifies that `CanShowPromo()` returns `expected_result`.
  auto QueryIPH(const base::Feature& iph_feature,
                user_education::FeaturePromoResult expected_result) {
    std::ostringstream oss;
    oss << "QueryIPH(" << iph_feature.name << ", " << expected_result << ")";
    return CheckResult(
        [this, &iph_feature]() {
          return promo_controller()->CanShowPromo(iph_feature);
        },
        expected_result, oss.str());
  }

  auto UseFeature(const base::Feature& iph_feature) {
    return Steps(Do(base::BindLambdaForTesting([this, &iph_feature]() {
      EXPECT_TRUE(promo_controller()->IsPromoActive(iph_feature));
      promo_controller()->EndPromo(iph_feature,
                                   EndFeaturePromoReason::kFeatureEngaged);

      EXPECT_FALSE(promo_controller()->IsPromoActive(iph_feature));
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
          EXPECT_EQ(
              abort_count,
              user_action_tester_.GetActionCount(
                  std::string("UserEducation.MessageAction.AbortedByFeature.")
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
              action_name, static_cast<int>(FeaturePromoClosedReason::kDismiss),
              dismiss_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(FeaturePromoClosedReason::kSnooze),
              snooze_count);
          histogram_tester_.ExpectBucketCount(
              action_name,
              static_cast<int>(FeaturePromoClosedReason::kAbortedByFeature),
              abort_count);
          histogram_tester_.ExpectBucketCount(
              action_name,
              static_cast<int>(FeaturePromoClosedReason::kFeatureEngaged),
              feature_engaged_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(FeaturePromoClosedReason::kAction),
              custom_action_count);

          return !testing::Test::HasNonfatalFailure();
        }),
        "Metrics");
  }

  user_education::FeaturePromoController* promo_controller() const {
    return static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoControllerForTesting());
  }

 private:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest, LogsAbortMetrics) {
  RunTestSequence(MaybeShowPromo(kToastTestFeature),
                  AbortPromo(kToastTestFeature),
                  CheckMetrics(kToastTestFeature,
                               /*snooze_count*/ 0,
                               /*dismiss_count*/ 0,
                               /*abort_count*/ 1,
                               /*feature_engaged_count*/ 0,
                               /*custom_action_count*/ 0));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       LogsEngagedMetrics) {
  RunTestSequence(MaybeShowPromo(kToastTestFeature),
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
  RunTestSequence(MaybeShowPromo(kCustomActionTestFeature),
                  PressNonDefaultPromoButton(),
                  CheckMetrics(kCustomActionTestFeature,
                               /*snooze_count*/ 0,
                               /*dismiss_count*/ 0,
                               /*abort_count*/ 0,
                               /*feature_engaged_count*/ 0,
                               /*custom_action_count*/ 1));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CanShowPromoReturnsExpectedValue) {
  RunTestSequence(
      QueryIPH(kToastTestFeature,
               user_education::FeaturePromoResult::Success()),
      MaybeShowPromo(kToastTestFeature),
      QueryIPH(kToastTestFeature,
               user_education::FeaturePromoResult::kBlockedByPromo),
      PressClosePromoButton(),
      QueryIPH(kToastTestFeature,
               user_education::FeaturePromoResult::kPermanentlyDismissed));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterCancel) {
  bool called = false;
  FeaturePromoClosedReason close_reason = FeaturePromoClosedReason::kAbortPromo;

  user_education::FeaturePromoParams params(kCustomActionTestFeature);
  params.close_callback =
      base::BindLambdaForTesting([this, &called, &close_reason]() {
        called = true;
        EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
            user_education::FeaturePromoParams(kCustomActionTestFeature),
            &close_reason));
      });

  RunTestSequence(
      MaybeShowPromo(std::move(params),
                     user_education::FeaturePromoResult::Success()),
      PressClosePromoButton(), CheckVariable(called, true),
      CheckVariable(close_reason, FeaturePromoClosedReason::kCancel));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterConfirm) {
  bool called = false;
  FeaturePromoClosedReason close_reason = FeaturePromoClosedReason::kAbortPromo;

  user_education::FeaturePromoParams params(kCustomActionTestFeature);
  params.close_callback =
      base::BindLambdaForTesting(
          [this, &called, &close_reason]() {
            called = true;
            EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
                kCustomActionTestFeature, &close_reason));
          });

  RunTestSequence(
      MaybeShowPromo(std::move(params),
                     user_education::FeaturePromoResult::Success()),
      PressDefaultPromoButton(), CheckVariable(called, true),
      CheckVariable(close_reason, FeaturePromoClosedReason::kDismiss));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterCustomAction) {
  bool called = false;
  FeaturePromoClosedReason close_reason = FeaturePromoClosedReason::kAbortPromo;

  user_education::FeaturePromoParams params(kLegalNoticeTestFeature);
  params.close_callback =
      base::BindLambdaForTesting(
          [this, &called, &close_reason]() {
            // Normal promos will defer writing close data until the promo is
            // fully ended.
            called = true;
            EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
                kLegalNoticeTestFeature, &close_reason));
          });

  RunTestSequence(
      MaybeShowPromo(std::move(params),
                     user_education::FeaturePromoResult::Success()),
      PressNonDefaultPromoButton(), CheckVariable(called, true),
      CheckVariable(close_reason, FeaturePromoClosedReason::kAction));
}

// Using the base interactive browser test re-enables window activation
// checking.
using BrowserFeaturePromoControllerActivationUiTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerActivationUiTest,
                       CanShowPromoForElement) {
  auto widget = std::make_unique<views::Widget>();

  auto can_show_promo = [this](ui::TrackedElement* anchor) {
    return static_cast<BrowserFeaturePromoController*>(
               browser()->window()->GetFeaturePromoControllerForTesting())
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
                     views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);
                 params.context = browser_view->GetWidget()->GetNativeWindow();
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
