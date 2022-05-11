// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_snooze_service.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/style/platform_style.h"
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
base::Feature kTutorialIPHFeature{"SecondIPHFeature",
                                  base::FEATURE_ENABLED_BY_DEFAULT};
constexpr char kTestTutorialIdentifier[] = "Test Tutorial";
}  // namespace

using user_education::FeaturePromoController;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoSnoozeService;
using user_education::FeaturePromoSpecification;
using user_education::HelpBubble;
using user_education::HelpBubbleArrow;
using user_education::HelpBubbleFactoryRegistry;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;
using user_education::HelpBubbleViews;
using user_education::TutorialDescription;

class BrowserFeaturePromoControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    controller_ = browser_view()->GetFeaturePromoController();
    lock_ = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                profile()));

    registry()->ClearFeaturesForTesting();

    // Register placeholder tutorials and IPH journeys.

    auto* const user_education_service =
        UserEducationServiceFactory::GetForProfile(browser()->profile());

    // Create a dummy tutorial.
    // This is just the first two steps of the "create tab group" tutorial.
    TutorialDescription desc;

    TutorialDescription::Step step1(0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                    ui::InteractionSequence::StepType::kShown,
                                    kTabStripElementId, std::string(),
                                    HelpBubbleArrow::kTopCenter);
    desc.steps.emplace_back(step1);

    TutorialDescription::Step step2(
        0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
        ui::InteractionSequence::StepType::kShown, kTabGroupEditorBubbleId,
        std::string(), HelpBubbleArrow::kLeftCenter,
        ui::CustomElementEventType(), false /*must_remain_visible*/);
    desc.steps.emplace_back(std::move(step2));

    user_education_service->tutorial_registry().AddTutorial(
        kTestTutorialIdentifier, std::move(desc));

    user_education_service->feature_promo_registry().RegisterFeature(
        DefaultBubbleParams());

    user_education_service->feature_promo_registry().RegisterFeature(
        FeaturePromoSpecification::CreateForTutorialPromo(
            kTutorialIPHFeature, kAppMenuButtonElementId, IDS_REOPEN_TAB_PROMO,
            kTestTutorialIdentifier));

    // Make sure the browser view is visible for the tests.
    browser_view()->GetWidget()->Show();
  }

  void TearDown() override {
    TestWithBrowserView::TearDown();
    lock_.reset();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(
            BrowserFeaturePromoControllerTest::MakeTestTracker));
    return factories;
  }

 protected:
  FeaturePromoSnoozeService* snooze_service() {
    return controller_->snooze_service();
  }

  FeaturePromoRegistry* registry() { return controller_->registry(); }

  HelpBubbleFactoryRegistry* bubble_factory() {
    return controller_->bubble_factory_registry();
  }

  HelpBubbleView* GetPromoBubble(HelpBubble* bubble) {
    return bubble ? bubble->AsA<HelpBubbleViews>()->bubble_view() : nullptr;
  }

  HelpBubbleView* GetPromoBubble() {
    return GetPromoBubble(controller_->promo_bubble());
  }

  HelpBubbleView* GetCriticalPromoBubble() {
    return GetPromoBubble(controller_->critical_promo_bubble());
  }

  views::View* GetAnchorView() {
    return browser_view()->toolbar()->app_menu_button();
  }

  ui::TrackedElement* GetAnchorElement() {
    auto* const result =
        views::ElementTrackerViews::GetInstance()->GetElementForView(
            GetAnchorView());
    CHECK(result);
    return result;
  }

  FeaturePromoSpecification DefaultBubbleParams() {
    return FeaturePromoSpecification::CreateForLegacyPromo(
        &kTestIPHFeature, kAppMenuButtonElementId, IDS_REOPEN_TAB_PROMO);
  }

  raw_ptr<BrowserFeaturePromoController> controller_;
  raw_ptr<NiceMock<feature_engagement::test::MockTracker>> mock_tracker_;
  BrowserFeaturePromoController::TestLock lock_;

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

using BubbleCloseCallback = BrowserFeaturePromoController::BubbleCloseCallback;

TEST_F(BrowserFeaturePromoControllerTest, GetForView) {
  EXPECT_EQ(controller_,
            BrowserFeaturePromoController::GetForView(GetAnchorView()));

  // For a view not in the BrowserView's hierarchy, it should return null.
  views::View orphan_view;
  EXPECT_EQ(nullptr, BrowserFeaturePromoController::GetForView(&orphan_view));
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendToShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<BubbleCloseCallback> close_callback;
  EXPECT_CALL(close_callback, Run()).Times(0);

  EXPECT_FALSE(
      controller_->MaybeShowPromo(kTestIPHFeature, {}, close_callback.Get()));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubble) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest,
       DismissNonCriticalBubbleInRegion_RegionDoesNotOverlap) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  const gfx::Rect bounds =
      GetPromoBubble()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect non_overlapping_region(bounds.right() + 1, bounds.bottom() + 1, 10,
                                   10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(non_overlapping_region);
  EXPECT_FALSE(result);
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       DismissNonCriticalBubbleInRegion_RegionOverlaps) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  const gfx::Rect bounds =
      GetPromoBubble()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_TRUE(result);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       DismissNonCriticalBubbleInRegion_CriticalPromo) {
  const auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);
  const gfx::Rect bounds =
      GetPromoBubble(bubble.get())->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_FALSE(result);
  EXPECT_TRUE(bubble->is_open());
}

TEST_F(BrowserFeaturePromoControllerTest, SnoozeServiceBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  snooze_service()->OnUserDismiss(kTestIPHFeature);
  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
  snooze_service()->Reset(kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsWhenRequested) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, {}, close_callback.Get()));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run()).Times(1);

  EXPECT_TRUE(controller_->CloseBubble(kTestIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfPromoNotShowing) {
  EXPECT_FALSE(controller_->CloseBubble(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfDifferentPromoShowing) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  EXPECT_FALSE(controller_->CloseBubble(kTutorialIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsOnBubbleClosure) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, {}, close_callback.Get()));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run());
  bubble->GetWidget()->Close();
  widget_observer.Wait();

  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest,
       ContinuedPromoDefersBackendDismissed) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  base::MockCallback<BubbleCloseCallback> close_callback;
  ASSERT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, {}, close_callback.Get()));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  // First check that CloseBubbleAndContinuePromo() actually closes the
  // bubble, but doesn't yet tell the backend the promo finished.

  EXPECT_CALL(close_callback, Run()).Times(1);
  FeaturePromoController::PromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature,
                                         /* include_continued_promos =*/true));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();

  // Check handle destruction causes the backend to be notified.

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
}

TEST_F(BrowserFeaturePromoControllerTest, PromoHandleDismissesPromoOnRelease) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoController::PromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.
  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature,
                                          /* include_continued_promos =*/true));
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoOnOverwrite) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoController::PromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle = FeaturePromoController::PromoHandle();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoExactlyOnce) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoController::PromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
  promo_handle.Release();
  EXPECT_FALSE(promo_handle);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoAfterMoveConstruction) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoController::PromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoController::PromoHandle promo_handle2(std::move(promo_handle));
  EXPECT_TRUE(promo_handle2);
  EXPECT_FALSE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle2.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle2);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoAfterMoveAssignment) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoController::PromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoController::PromoHandle promo_handle2;
  promo_handle2 = std::move(promo_handle);
  EXPECT_TRUE(promo_handle2);
  EXPECT_FALSE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle2.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle2);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PropertySetOnAnchorViewWhileBubbleOpen) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_FALSE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));

  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_TRUE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));

  controller_->CloseBubble(kTestIPHFeature);
  EXPECT_FALSE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));
}

TEST_F(BrowserFeaturePromoControllerTest, TestCanBlockPromos) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);

  auto lock = controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, TestCanStopCurrentPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  auto lock = controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, CriticalPromoBlocksNormalPromo) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(GetCriticalPromoBubble());

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));

  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, CriticalPromoPreemptsNormalPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<BubbleCloseCallback> close_callback;
  EXPECT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, {}, close_callback.Get()));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_CALL(close_callback, Run()).Times(1);

  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, FirstCriticalPromoHasPrecedence) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);

  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());

  auto bubble2 =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_FALSE(bubble2);
  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, CloseBubbleForCriticalPromo) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);

  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
  bubble->Close();
  EXPECT_FALSE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleForCriticalPromoDoesNothingAfterClose) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);
  ASSERT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
  auto* widget = GetPromoBubble(bubble.get())->GetWidget();
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();

  EXPECT_FALSE(GetCriticalPromoBubble());

  bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowNewCriticalPromoAfterClose) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(GetCriticalPromoBubble());
  bubble->Close();
  EXPECT_FALSE(GetCriticalPromoBubble());

  bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, FailsIfBubbleIsShowing) {
  HelpBubbleParams bubble_params;
  bubble_params.body_text = l10n_util::GetStringUTF16(IDS_REOPEN_TAB_PROMO);
  auto bubble = bubble_factory()->CreateHelpBubble(GetAnchorElement(),
                                                   std::move(bubble_params));
  EXPECT_TRUE(bubble);

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));
}

// Test that a feature promo can chain into a tutorial.
TEST_F(BrowserFeaturePromoControllerTest, StartsTutorial) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kTutorialIPHFeature));

  // Simulate clicking the "Show Tutorial" button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  // We should be running the tutorial now.
  auto& tutorial_service =
      UserEducationServiceFactory::GetForProfile(browser()->profile())
          ->tutorial_service();
  EXPECT_TRUE(tutorial_service.IsRunningTutorial());
  tutorial_service.AbortTutorial(absl::nullopt);
}

TEST_F(BrowserFeaturePromoControllerTest, GetAnchorContext) {
  EXPECT_EQ(browser_view()->GetElementContext(),
            controller_->GetAnchorContext());
}

TEST_F(BrowserFeaturePromoControllerTest, GetAcceleratorProvider) {
  EXPECT_EQ(browser_view(), controller_->GetAcceleratorProvider());
}

TEST_F(BrowserFeaturePromoControllerTest, GetFocusHelpBubbleScreenReaderHint) {
  EXPECT_TRUE(controller_
                  ->GetFocusHelpBubbleScreenReaderHint(
                      FeaturePromoSpecification::PromoType::kToast,
                      GetAnchorElement(), false)
                  .empty());
  EXPECT_FALSE(controller_
                   ->GetFocusHelpBubbleScreenReaderHint(
                       FeaturePromoSpecification::PromoType::kSnooze,
                       GetAnchorElement(), false)
                   .empty());

  // Target element is focusable so critical promo should also have a hint.
  EXPECT_FALSE(controller_
                   ->GetFocusHelpBubbleScreenReaderHint(
                       FeaturePromoSpecification::PromoType::kLegacy,
                       GetAnchorElement(), true)
                   .empty());
}
