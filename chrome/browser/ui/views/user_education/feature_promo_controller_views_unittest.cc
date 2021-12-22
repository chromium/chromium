// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"

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
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service_manager.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner_impl.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_border.h"
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

  FeaturePromoSpecification DefaultBubbleParams() {
    return FeaturePromoSpecification::CreateForLegacyPromo(
        &kTestIPHFeature, kAppMenuButtonElementId, IDS_REOPEN_TAB_PROMO);
  }

  FeaturePromoSpecification IPHSnoozeBubbleParams() {
    return FeaturePromoSpecification::CreateForSnoozePromo(
        kTestIPHFeature, kAppMenuButtonElementId, IDS_REOPEN_TAB_PROMO);
  }

  raw_ptr<FeaturePromoControllerViews> controller_;
  raw_ptr<NiceMock<feature_engagement::test::MockTracker>> mock_tracker_;

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

  EXPECT_FALSE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView(), {}, close_callback.Get()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, ShowsBubble) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));
  EXPECT_TRUE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest,
       DismissNonCriticalBubbleInRegion_RegionDoesNotOverlap) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));

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
  EXPECT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));

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
  EXPECT_FALSE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));
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
  ASSERT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView(), {}, close_callback.Get()));

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
  ASSERT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));

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
  ASSERT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView(), {}, close_callback.Get()));

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
  ASSERT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView(), {}, close_callback.Get()));

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

  ASSERT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));
  EXPECT_TRUE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));

  controller_->CloseBubble(kTestIPHFeature);
  EXPECT_FALSE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));
}

TEST_F(FeaturePromoControllerViewsTest, GetsParamsFromRegistry) {
  // If the browser view is not visible, none of the element tracker stuff will
  // work properly, so ensure the browser widget is visible.
  browser_view()->GetWidget()->Show();
  FeaturePromoRegistry::GetInstance()->RegisterFeature(DefaultBubbleParams());

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
  EXPECT_FALSE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));
  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_FALSE(
      FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, TestCanStopCurrentPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));

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
  EXPECT_FALSE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));

  EXPECT_FALSE(controller_->BubbleIsShowing(kTestIPHFeature));
  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->bubble_for_testing());
}

TEST_F(FeaturePromoControllerViewsTest, CriticalPromoPreemptsNormalPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<BubbleCloseCallback> close_callback;
  EXPECT_TRUE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView(), {}, close_callback.Get()));
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
  bubble_params.body_text = l10n_util::GetStringUTF16(IDS_REOPEN_TAB_PROMO);

  EXPECT_TRUE(FeaturePromoBubbleOwnerImpl::GetInstance()->ShowBubble(
      std::move(bubble_params), base::DoNothing()));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  EXPECT_FALSE(controller_->MaybeShowPromoFromSpecification(
      DefaultBubbleParams(), GetAnchorView()));
}

// Test that a feature promo can chain into a tutorial.
TEST_F(FeaturePromoControllerViewsTest, StartsTutorial) {
  TutorialService* const tutorial_service =
      TutorialServiceManager::GetInstance()->GetTutorialServiceForProfile(
          browser()->profile());
  if (!tutorial_service)
    return;

  // Create a dummy tutorial.
  // This is just the first two steps of the "create tab group" tutorial.
  constexpr char kTestTutorialIdentifier[] = "Test Tutorial";
  TutorialDescription desc;

  TutorialDescription::Step step1(
      absl::nullopt,
      u"Right Click on a Tab and select \"Add Tab To new Group\".",
      ui::InteractionSequence::StepType::kShown, kTabStripElementId,
      std::string(), TutorialDescription::Step::Arrow::TOP, absl::nullopt);
  desc.steps.emplace_back(step1);

  TutorialDescription::Step step2(
      absl::nullopt, u"Select \"Enter a name for your Tab Group\".",
      ui::InteractionSequence::StepType::kShown,
      TabGroupEditorBubbleView::kEditorBubbleIdentifier, std::string(),
      TutorialDescription::Step::Arrow::CENTER_HORIZONTAL,
      false /*must_remain_visible*/);
  desc.steps.emplace_back(std::move(step2));

  TutorialServiceManager::GetInstance()->tutorial_registry()->AddTutorial(
      kTestTutorialIdentifier, desc);

  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(1)
      .WillOnce(Return(true));
  FeaturePromoSpecification spec =
      FeaturePromoSpecification::CreateForTutorialPromo(
          kTestIPHFeature, kAppMenuButtonElementId, IDS_REOPEN_TAB_PROMO,
          kTestTutorialIdentifier);
  ASSERT_TRUE(
      controller_->MaybeShowPromoFromSpecification(spec, GetAnchorView(), {}));

  // Simulate clicking the "Show Tutorial" button.
  auto* const bubble = static_cast<FeaturePromoBubbleOwnerImpl*>(
                           controller_->bubble_owner_for_testing())
                           ->bubble_for_testing();
  ASSERT_TRUE(bubble);
  auto* const button = bubble->GetButtonForTesting(
      views::PlatformStyle::kIsOkButtonLeading ? 0 : 1);
  ASSERT_TRUE(button);
  ui::MouseEvent mouse_press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent mouse_release(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  button->OnMouseEvent(&mouse_press);
  button->OnMouseEvent(&mouse_release);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  waiter.Wait();

  // We should be running the tutorial now.
  EXPECT_TRUE(tutorial_service->IsRunningTutorial());
  tutorial_service->HideCurrentBubbleIfShowing();
}
