// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner_impl.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

namespace {
base::Feature kTestIPHFeature{"TestIPHFeature",
                              base::FEATURE_ENABLED_BY_DEFAULT};
base::Feature kSecondIPHFeature{"SecondIPHFeature",
                                base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace

class FeaturePromoControllerViewsTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    controller_ = browser_view()->feature_promo_controller();
    FeaturePromoControllerViews::BlockActiveWindowCheckForTesting();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                profile()));

    FeaturePromoRegistry::GetInstance()->ClearFeaturesForTesting();
  }

  void TearDown() override {
    FeaturePromoRegistry::GetInstance()->ReinitializeForTesting();
    TestWithBrowserView::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(FeaturePromoControllerViewsTest::MakeTestTracker));
    return factories;
  }

 protected:
  views::View* GetAnchorView() {
    return browser_view()->toolbar()->app_menu_button();
  }

  FeaturePromoBubbleParams DefaultBubbleParams() {
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_REOPEN_TAB_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;
    return params;
  }

  FeaturePromoBubbleParams IPHSnoozeBubbleParams() {
    FeaturePromoBubbleParams params = DefaultBubbleParams();
    params.allow_snooze = true;
    return params;
  }

  FeaturePromoControllerViews* controller_;
  NiceMock<feature_engagement::test::MockTracker>* mock_tracker_;

 private:
  static std::unique_ptr<KeyedService> MakeTestTracker(
      content::BrowserContext* context) {
    auto tracker =
        std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

    // Allow other code to call into the tracker.
    EXPECT_CALL(*tracker, NotifyEvent(_)).Times(AnyNumber());
    EXPECT_CALL(*tracker, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    return tracker;
  }
};

using BubbleCloseCallback = FeaturePromoControllerViews::BubbleCloseCallback;

TEST_F(FeaturePromoControllerViewsTest, GetForView) {
  EXPECT_EQ(controller_,
            FeaturePromoControllerViews::GetForView(GetAnchorView()));

  // For a view not in the BrowserView's hierarchy, it should return null.
  views::View orphan_view;
  EXPECT_EQ(nullptr, FeaturePromoControllerViews::GetForView(&orphan_view));
}

TEST_F(FeaturePromoControllerViewsTest, AsksBackendToShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<BubbleCloseCallback> close_callback;
  EXPECT_CALL(close_callback, Run()).Times(0);

  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView(),
      close_callback.Get()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, ShowsBubble) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest,
       DismissNonCriticalBubbleInRegion_RegionDoesNotOverlap) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));

  const gfx::Rect bounds = FeaturePromoBubbleOwnerImpl::GetInstance()
                               ->bubble_for_testing()
                               ->GetWidget()
                               ->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect non_overlapping_region(bounds.right() + 1, bounds.bottom() + 1, 10,
                                   10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(non_overlapping_region);
  EXPECT_FALSE(result);
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
}

TEST_F(FeaturePromoControllerViewsTest,
       DismissNonCriticalBubbleInRegion_RegionOverlaps) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));

  const gfx::Rect bounds = FeaturePromoBubbleOwnerImpl::GetInstance()
                               ->bubble_for_testing()
                               ->GetWidget()
                               ->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_TRUE(result);
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
}

TEST_F(FeaturePromoControllerViewsTest,
       DismissNonCriticalBubbleInRegion_CriticalPromo) {
  const auto token =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView());
  ASSERT_TRUE(token.has_value());
  const gfx::Rect bounds = FeaturePromoBubbleOwnerImpl::GetInstance()
                               ->bubble_for_testing()
                               ->GetWidget()
                               ->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_FALSE(result);
  EXPECT_TRUE(controller_->CriticalPromoIsShowing(token.value()));
}

TEST_F(FeaturePromoControllerViewsTest, SnoozeServiceBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  controller_->snooze_service_for_testing()->OnUserDismiss(kTestIPHFeature);
  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
  controller_->snooze_service_for_testing()->Reset(kTestIPHFeature);
}

TEST_F(FeaturePromoControllerViewsTest, PromoEndsWhenRequested) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView(),
      close_callback.Get()));

  // Only valid before the widget is closed.
  FeaturePromoBubbleView* const bubble =
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run()).Times(1);

  EXPECT_TRUE(controller_->CloseBubble(kTestIPHFeature));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());

  // Ensure the widget does close.
  widget_observer.Wait();
}

TEST_F(FeaturePromoControllerViewsTest,
       CloseBubbleDoesNothingIfPromoNotShowing) {
  EXPECT_FALSE(controller_->CloseBubble(kTestIPHFeature));
}

TEST_F(FeaturePromoControllerViewsTest,
       CloseBubbleDoesNothingIfDifferentPromoShowing) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));

  EXPECT_FALSE(controller_->CloseBubble(kSecondIPHFeature));
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, PromoEndsOnBubbleClosure) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView(),
      close_callback.Get()));

  // Only valid before the widget is closed.
  FeaturePromoBubbleView* const bubble =
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run());
  bubble->GetWidget()->Close();
  widget_observer.Wait();

  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, ContinuedPromoDefersBackendDismissed) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView(),
      close_callback.Get()));

  // Only valid before the widget is closed.
  FeaturePromoBubbleView* const bubble =
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  // First check that CloseBubbleAndContinuePromo() actually closes the
  // bubble, but doesn't yet tell the backend the promo finished.

  EXPECT_CALL(close_callback, Run()).Times(1);
  absl::optional<FeaturePromoController::PromoHandle> promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());

  // Ensure the widget does close.
  widget_observer.Wait();

  // Check handle destruction causes the backend to be notified.

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle.reset();
}

TEST_F(FeaturePromoControllerViewsTest,
       PropertySetOnAnchorViewWhileBubbleOpen) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_FALSE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));

  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));
  EXPECT_TRUE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));

  controller_->CloseBubble(kTestIPHFeature);
  EXPECT_FALSE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));
}

TEST_F(FeaturePromoControllerViewsTest, GetsParamsFromRegistry) {
  FeaturePromoBubbleParams params = DefaultBubbleParams();
  FeaturePromoRegistry::GetInstance()->RegisterFeature(
      kTestIPHFeature, DefaultBubbleParams(),
      base::BindRepeating([](BrowserView* browser_view) {
        return static_cast<views::View*>(
            browser_view->toolbar()->app_menu_button());
      }));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_EQ(browser_view()->toolbar()->app_menu_button(),
            FeaturePromoBubbleOwnerImpl::GetInstance()
                ->bubble_for_testing()
                ->GetAnchorView());
}

TEST_F(FeaturePromoControllerViewsTest, TestCanBlockPromos) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);

  controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, TestCanStopCurrentPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));

  controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, CriticalPromoBlocksNormalPromo) {
  EXPECT_TRUE(
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView()));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));

  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, CriticalPromoPreemptsNormalPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<BubbleCloseCallback> close_callback;
  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView(),
      close_callback.Get()));
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run()).Times(1);

  EXPECT_TRUE(
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, FirstCriticalPromoHasPrecedence) {
  EXPECT_TRUE(
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView()));

  const auto* first_bubble =
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing();
  EXPECT_TRUE(first_bubble);

  EXPECT_FALSE(
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView()));
  EXPECT_EQ(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing(),
            first_bubble);
}

TEST_F(FeaturePromoControllerViewsTest, CloseBubbleForCriticalPromo) {
  absl::optional<base::Token> maybe_id =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView());
  ASSERT_TRUE(maybe_id);
  base::Token id = maybe_id.value();

  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
  controller_->CloseBubbleForCriticalPromo(id);
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest,
       CloseBubbleForCriticalPromoDoesNothingAfterClose) {
  absl::optional<base::Token> maybe_id =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView());
  ASSERT_TRUE(maybe_id);
  base::Token id = maybe_id.value();

  auto* bubble =
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing();
  ASSERT_TRUE(bubble);
  bubble->GetWidget()->Close();
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());

  EXPECT_TRUE(
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView()));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());

  // Since |id| has expired, this should do nothing.
  controller_->CloseBubbleForCriticalPromo(id);
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, ShowNewCriticalPromoAfterClose) {
  absl::optional<base::Token> maybe_id =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView());
  ASSERT_TRUE(maybe_id);
  base::Token id = maybe_id.value();

  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
  controller_->CloseBubbleForCriticalPromo(id);
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());

  EXPECT_TRUE(
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorView()));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, FailsIfBubbleIsShowing) {
  FeaturePromoBubbleView::CreateParams bubble_params;
  bubble_params.anchor_view = GetAnchorView();
  bubble_params.body_text = IDS_REOPEN_TAB_PROMO;

  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->ShowBubble(
      std::move(bubble_params), base::DoNothing()));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), GetAnchorView()));
}

// Test that IPH defaults are respected in the Snooze case.
TEST_F(FeaturePromoControllerViewsTest, IPHSnoozeUniqueTimeout) {
  FeaturePromoBubbleView::CreateParams bubble_params =
      controller_->GetBaseCreateParams(IPHSnoozeBubbleParams(),
                                       GetAnchorView());
  EXPECT_EQ(FeaturePromoSnoozeService::kTimeoutNoInteraction,
            bubble_params.timeout_no_interaction);
  EXPECT_EQ(FeaturePromoSnoozeService::kTimeoutAfterInteraction,
            bubble_params.timeout_after_interaction);
}
