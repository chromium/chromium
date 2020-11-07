// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/test/widget_test.h"

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
    params.anchor_view = GetAnchorView();
    params.arrow = views::BubbleBorder::TOP_RIGHT;
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
      kTestIPHFeature, DefaultBubbleParams(), close_callback.Get()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, ShowsBubble) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(kTestIPHFeature,
                                                    DefaultBubbleParams()));
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, SnoozeServiceBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  controller_->snooze_service_for_testing()->OnUserDismiss(kTestIPHFeature);
  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(kTestIPHFeature,
                                                     DefaultBubbleParams()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(controller_->promo_bubble_for_testing());
  controller_->snooze_service_for_testing()->Reset(kTestIPHFeature);
}

TEST_F(FeaturePromoControllerViewsTest, PromoEndsWhenRequested) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), close_callback.Get()));

  // Only valid before the widget is closed.
  FeaturePromoBubbleView* const bubble =
      controller_->promo_bubble_for_testing();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  views::test::WidgetClosingObserver widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run()).Times(1);

  EXPECT_TRUE(controller_->CloseBubble(kTestIPHFeature));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(controller_->promo_bubble_for_testing());

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
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(kTestIPHFeature,
                                                    DefaultBubbleParams()));

  EXPECT_FALSE(controller_->CloseBubble(kSecondIPHFeature));
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, PromoEndsOnBubbleClosure) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), close_callback.Get()));

  // Only valid before the widget is closed.
  FeaturePromoBubbleView* const bubble =
      controller_->promo_bubble_for_testing();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  views::test::WidgetClosingObserver widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run());
  bubble->GetWidget()->Close();
  widget_observer.Wait();

  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, ContinuedPromoDefersBackendDismissed) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), close_callback.Get()));

  // Only valid before the widget is closed.
  FeaturePromoBubbleView* const bubble =
      controller_->promo_bubble_for_testing();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  views::test::WidgetClosingObserver widget_observer(bubble->GetWidget());

  // First check that CloseBubbleAndContinuePromo() actually closes the
  // bubble, but doesn't yet tell the backend the promo finished.

  EXPECT_CALL(close_callback, Run()).Times(1);
  base::Optional<FeaturePromoController::PromoHandle> promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(controller_->promo_bubble_for_testing());

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

  ASSERT_TRUE(controller_->MaybeShowPromoWithParams(kTestIPHFeature,
                                                    DefaultBubbleParams()));
  EXPECT_TRUE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));

  controller_->CloseBubble(kTestIPHFeature);
  EXPECT_FALSE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));
}

TEST_F(FeaturePromoControllerViewsTest, GetsParamsFromRegistry) {
  FeaturePromoBubbleParams params = DefaultBubbleParams();
  params.anchor_view = nullptr;
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
            controller_->promo_bubble_for_testing()->GetAnchorView());
}

TEST_F(FeaturePromoControllerViewsTest, TestCanBlockPromos) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);

  controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(kTestIPHFeature,
                                                     DefaultBubbleParams()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, TestCanStopCurrentPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(kTestIPHFeature,
                                                    DefaultBubbleParams()));

  controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, CriticalPromoBlocksNormalPromo) {
  EXPECT_TRUE(controller_->ShowCriticalPromo(DefaultBubbleParams()));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_FALSE(controller_->MaybeShowPromoWithParams(kTestIPHFeature,
                                                     DefaultBubbleParams()));

  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, CriticalPromoPreemptsNormalPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<BubbleCloseCallback> close_callback;
  EXPECT_TRUE(controller_->MaybeShowPromoWithParams(
      kTestIPHFeature, DefaultBubbleParams(), close_callback.Get()));
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run()).Times(1);

  EXPECT_TRUE(controller_->ShowCriticalPromo(DefaultBubbleParams()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, FirstCriticalPromoHasPrecedence) {
  EXPECT_TRUE(controller_->ShowCriticalPromo(DefaultBubbleParams()));

  const auto* first_bubble = controller_->promo_bubble_for_testing();
  EXPECT_TRUE(first_bubble);

  EXPECT_FALSE(controller_->ShowCriticalPromo(DefaultBubbleParams()));
  EXPECT_EQ(controller_->promo_bubble_for_testing(), first_bubble);
}

TEST_F(FeaturePromoControllerViewsTest, CloseBubbleForCriticalPromo) {
  base::Optional<base::Token> maybe_id =
      controller_->ShowCriticalPromo(DefaultBubbleParams());
  ASSERT_TRUE(maybe_id);
  base::Token id = maybe_id.value();

  EXPECT_TRUE(controller_->promo_bubble_for_testing());
  controller_->CloseBubbleForCriticalPromo(id);
  EXPECT_FALSE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest,
       CloseBubbleForCriticalPromoDoesNothingAfterClose) {
  base::Optional<base::Token> maybe_id =
      controller_->ShowCriticalPromo(DefaultBubbleParams());
  ASSERT_TRUE(maybe_id);
  base::Token id = maybe_id.value();

  auto* bubble = controller_->promo_bubble_for_testing();
  ASSERT_TRUE(bubble);
  bubble->GetWidget()->Close();
  EXPECT_FALSE(controller_->promo_bubble_for_testing());

  EXPECT_TRUE(controller_->ShowCriticalPromo(DefaultBubbleParams()));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());

  // Since |id| has expired, this should do nothing.
  controller_->CloseBubbleForCriticalPromo(id);
  EXPECT_TRUE(controller_->promo_bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, ShowNewCriticalPromoAfterClose) {
  base::Optional<base::Token> maybe_id =
      controller_->ShowCriticalPromo(DefaultBubbleParams());
  ASSERT_TRUE(maybe_id);
  base::Token id = maybe_id.value();

  EXPECT_TRUE(controller_->promo_bubble_for_testing());
  controller_->CloseBubbleForCriticalPromo(id);
  EXPECT_FALSE(controller_->promo_bubble_for_testing());

  EXPECT_TRUE(controller_->ShowCriticalPromo(DefaultBubbleParams()));
  EXPECT_TRUE(controller_->promo_bubble_for_testing());
}
