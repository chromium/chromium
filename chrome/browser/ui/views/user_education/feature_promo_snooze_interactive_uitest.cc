// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/user_education/common/feature_promo_snooze_service.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

namespace {
BASE_FEATURE(kSnoozeTestFeature,
             "SnoozeTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
}

class FeaturePromoSnoozeInteractiveTest : public InteractiveBrowserTest {
 public:
  FeaturePromoSnoozeInteractiveTest() {
    subscription_ = BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(RegisterMockTracker));
    scoped_feature_list_.InitAndEnableFeatures({kSnoozeTestFeature});
  }
  ~FeaturePromoSnoozeInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                browser()->profile()));
    ASSERT_TRUE(mock_tracker_);

    promo_controller_ = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());
    snooze_service_ = promo_controller_->snooze_service();

    if (!promo_controller_->registry()->IsFeatureRegistered(
            kSnoozeTestFeature)) {
      promo_controller_->registry()->RegisterFeature(
          user_education::FeaturePromoSpecification::CreateForSnoozePromo(
              kSnoozeTestFeature, kToolbarAppMenuButtonElementId,
              IDS_TAB_GROUPS_NEW_GROUP_PROMO));
    }
  }

 protected:
  using SnoozeData = user_education::FeaturePromoSnoozeService::SnoozeData;

  auto CheckSnoozePrefs(bool is_dismissed, int show_count, int snooze_count) {
    return Check(base::BindLambdaForTesting(
        [this, is_dismissed, show_count, snooze_count]() {
          auto data = snooze_service_->ReadSnoozeData(kSnoozeTestFeature);

          if (!data.has_value()) {
            return false;
          }

          EXPECT_EQ(data->is_dismissed, is_dismissed);
          EXPECT_EQ(data->show_count, show_count);
          EXPECT_EQ(data->snooze_count, snooze_count);

          // last_show_time is only meaningful if a show has occurred.
          if (data->show_count > 0) {
            EXPECT_GE(data->last_show_time, last_show_time_.first);
            EXPECT_LE(data->last_show_time, last_show_time_.second);
          }

          // last_snooze_time is only meaningful if a snooze has occurred.
          if (data->snooze_count > 0) {
            EXPECT_GE(data->last_snooze_time, last_snooze_time_.first);
            EXPECT_LE(data->last_snooze_time, last_snooze_time_.second);
          }

          return !testing::Test::HasNonfatalFailure();
        }));
  }

  auto SetSnoozePrefs(const SnoozeData& data) {
    return Do(base::BindLambdaForTesting([this, data] {
      snooze_service_->SaveSnoozeData(kSnoozeTestFeature, data);
    }));
  }

  // Tries to show tab groups IPH by meeting the trigger conditions. If
  // |should_show| is true it checks that it was shown. If false, it
  // checks that it was not shown.
  auto AttemptIPH(bool should_show) {
    return Do(base::BindLambdaForTesting([this, should_show]() {
      if (should_show) {
        last_show_time_.first = base::Time::Now();
        EXPECT_CALL(*mock_tracker_,
                    ShouldTriggerHelpUI(Ref(kSnoozeTestFeature)))
            .WillOnce(Return(true));
      } else {
        EXPECT_CALL(*mock_tracker_,
                    ShouldTriggerHelpUI(Ref(kSnoozeTestFeature)))
            .Times(0);
      }

      ASSERT_EQ(should_show,
                promo_controller_->MaybeShowPromo(kSnoozeTestFeature));
      ASSERT_EQ(should_show,
                promo_controller_->IsPromoActive(kSnoozeTestFeature));

      // If shown, Tracker::Dismissed should be called eventually.
      if (should_show) {
        EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kSnoozeTestFeature)));
        last_show_time_.second = base::Time::Now();
      }
    }));
  }

  auto SnoozeIPH() {
    return Steps(
        Do(base::BindLambdaForTesting(
            [this]() { last_snooze_time_.first = base::Time::Now(); })),
        PressButton(
            user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        Do(base::BindLambdaForTesting(
            [this]() { last_snooze_time_.second = base::Time::Now(); })));
  }

  auto DismissIPH() {
    return Steps(
        PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  raw_ptr<NiceMock<feature_engagement::test::MockTracker>,
          AcrossTasksDanglingUntriaged>
      mock_tracker_;
  raw_ptr<BrowserFeaturePromoController, AcrossTasksDanglingUntriaged>
      promo_controller_;
  raw_ptr<user_education::FeaturePromoSnoozeService,
          AcrossTasksDanglingUntriaged>
      snooze_service_;

 private:
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

  std::pair<base::Time, base::Time> last_show_time_;
  std::pair<base::Time, base::Time> last_snooze_time_;

  feature_engagement::test::ScopedIphFeatureList scoped_feature_list_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DismissDoesNotSnooze) {
  RunTestSequence(AttemptIPH(true), DismissIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ true,
                                   /* show_count */ 1,
                                   /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       SnoozeSetsCorrectTime) {
  RunTestSequence(AttemptIPH(true), SnoozeIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ false,
                                   /* show_count */ 1,
                                   /* snooze_count */ 1));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest, CanReSnooze) {
  // Simulate the user snoozing the IPH.
  SnoozeData data;
  data.is_dismissed = false;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_snooze_duration = base::Hours(26);
  data.last_snooze_time = base::Time::Now() - data.last_snooze_duration;
  data.last_show_time = data.last_snooze_time - base::Seconds(1);

  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(true), SnoozeIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ false,
                                   /* show_count */ 2,
                                   /* snooze_count */ 2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DoesNotShowIfDismissed) {
  SnoozeData data;
  data.is_dismissed = true;
  data.show_count = 1;
  data.snooze_count = 0;

  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(false));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DoesNotShowBeforeSnoozeDuration) {
  SnoozeData data;
  data.is_dismissed = false;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_snooze_duration = base::Hours(26);
  data.last_snooze_time = base::Time::Now();
  data.last_show_time = data.last_snooze_time - base::Seconds(1);

  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(false));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest, EndPromoSetsPrefs) {
  RunTestSequence(
      AttemptIPH(true), Do(base::BindLambdaForTesting([this]() {
        promo_controller_->EndPromo(kSnoozeTestFeature);
      })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       WidgetCloseSetsPrefs) {
  RunTestSequence(
      AttemptIPH(true),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               base::BindOnce([](user_education::HelpBubbleView* bubble) {
                 bubble->GetWidget()->CloseWithReason(
                     views::Widget::ClosedReason::kEscKeyPressed);
               })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest, AnchorHideSetsPrefs) {
  RunTestSequence(
      AttemptIPH(true),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               base::BindOnce([](user_education::HelpBubbleView* bubble) {
                 // This should yank the bubble out from under us.
                 bubble->GetAnchorView()->SetVisible(false);
               })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       WorkWithoutNonClickerData) {
  SnoozeData data;
  data.is_dismissed = false;
  data.snooze_count = 1;
  data.last_snooze_duration = base::Hours(26);
  data.last_snooze_time = base::Time::Now() - data.last_snooze_duration;

  // Non-clicker policy shipped pref entries that don't exist before.
  // Make sure empty entries are properly handled.
  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(true));
}
