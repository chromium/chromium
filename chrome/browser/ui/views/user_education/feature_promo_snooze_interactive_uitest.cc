// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class FeaturePromoSnoozeInteractiveTest : public InProcessBrowserTest {
 public:
  FeaturePromoSnoozeInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(
        feature_engagement::kIPHDesktopSnoozeFeature);

    subscription_ = BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(RegisterMockTracker));
  }

  void SetUpOnMainThread() override {
    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                browser()->profile()));
    ASSERT_TRUE(mock_tracker_);

    promo_controller_ = BrowserView::GetBrowserViewForBrowser(browser())
                            ->feature_promo_controller();
    snooze_service_ = promo_controller_->snooze_service_for_testing();
  }

 protected:
  void ClickButton(views::Button* button) {
    // TODO(crbug.com/1135850): switch back to MoveMouseToCenterAndPress when
    // fixed.
    ui::MouseEvent mouse_press(ui::ET_MOUSE_PRESSED, gfx::PointF(),
                               gfx::PointF(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
    button->OnMouseEvent(&mouse_press);

    ui::MouseEvent mouse_release(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                 gfx::PointF(), ui::EventTimeForNow(),
                                 ui::EF_LEFT_MOUSE_BUTTON,
                                 ui::EF_LEFT_MOUSE_BUTTON);
    button->OnMouseEvent(&mouse_release);
  }

  bool HasSnoozePrefs(const base::Feature& iph_feature) {
    return snooze_service_->ReadSnoozeData(iph_feature).has_value();
  }

  void CheckSnoozePrefs(const base::Feature& iph_feature,
                        bool is_dismissed,
                        int show_count,
                        int snooze_count,
                        base::Time last_show_time_min,
                        base::Time last_show_time_max,
                        base::Time last_snooze_time_min,
                        base::Time last_snooze_time_max) {
    auto data = snooze_service_->ReadSnoozeData(iph_feature);

    // If false, adds a failure and returns early from this function.
    ASSERT_TRUE(data.has_value());

    EXPECT_EQ(data->is_dismissed, is_dismissed);
    EXPECT_EQ(data->show_count, show_count);
    EXPECT_EQ(data->snooze_count, snooze_count);

    // last_show_time is only meaningful if a show has occurred.
    if (data->show_count > 0) {
      EXPECT_GE(data->last_show_time, last_show_time_min);
      EXPECT_LE(data->last_show_time, last_show_time_max);
    }

    // last_snooze_time is only meaningful if a snooze has occurred.
    if (data->snooze_count > 0) {
      EXPECT_GE(data->last_snooze_time, last_snooze_time_min);
      EXPECT_LE(data->last_snooze_time, last_snooze_time_max);
    }
  }

  void SetSnoozePrefs(const base::Feature& iph_feature,
                      bool is_dismissed,
                      base::Optional<int> show_count,
                      int snooze_count,
                      base::Optional<base::Time> last_show_time,
                      base::Time last_snooze_time,
                      base::TimeDelta last_snooze_duration) {
    FeaturePromoSnoozeService::SnoozeData data;
    data.is_dismissed = is_dismissed;
    if (show_count)
      data.show_count = *show_count;
    data.snooze_count = snooze_count;
    if (last_show_time)
      data.last_show_time = *last_show_time;
    data.last_snooze_time = last_snooze_time;
    data.last_snooze_duration = last_snooze_duration;
    snooze_service_->SaveSnoozeData(iph_feature, data);
  }

  // Tries to show tab groups IPH by meeting the trigger conditions. If
  // |should_show| is true it checks that it was shown. If false, it
  // checks that it was not shown.
  void AttemptTabGroupsIPH(bool should_show) {
    if (should_show) {
      EXPECT_CALL(*mock_tracker_,
                  ShouldTriggerHelpUI(Ref(
                      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
          .WillOnce(Return(true));
    } else {
      EXPECT_CALL(*mock_tracker_,
                  ShouldTriggerHelpUI(Ref(
                      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)))
          .Times(0);
    }

    // Opening 6 or more tabs is the triggering event for tab groups
    // IPH.
    for (int i = 0; i < 5; ++i)
      AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);

    ASSERT_EQ(should_show,
              promo_controller_->BubbleIsShowing(
                  feature_engagement::kIPHDesktopTabGroupsNewGroupFeature));

    // If shown, Tracker::Dismissed should be called eventually.
    if (should_show) {
      EXPECT_CALL(
          *mock_tracker_,
          Dismissed(
              Ref(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)));
    }
  }

  views::Button* GetSnoozeButtonForTesting(FeaturePromoBubbleView* promo) {
    return promo->GetButtonForTesting(
        views::PlatformStyle::kIsOkButtonLeading ? 1 : 0);
  }

  views::Button* GetDismissButtonForTesting(FeaturePromoBubbleView* promo) {
    return promo->GetButtonForTesting(
        views::PlatformStyle::kIsOkButtonLeading ? 0 : 1);
  }

  NiceMock<feature_engagement::test::MockTracker>* mock_tracker_;
  FeaturePromoControllerViews* promo_controller_;
  FeaturePromoSnoozeService* snooze_service_;

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

  base::test::ScopedFeatureList scoped_feature_list_;

  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DismissDoesNotSnooze) {
  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptTabGroupsIPH(true));
  base::Time show_time_max = base::Time::Now();

  FeaturePromoBubbleView* promo = promo_controller_->promo_bubble_for_testing();
  ClickButton(GetDismissButtonForTesting(promo));
  CheckSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                   /* is_dismiss */ true,
                   /* show_count */ 1,
                   /* snooze_count */ 0,
                   /* last_show_time_min */ show_time_min,
                   /* last_show_time_max */ show_time_max,
                   /* last_snooze_time_min */ base::Time(),
                   /* last_snooze_time_max */ base::Time());
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       SnoozeSetsCorrectTime) {
  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptTabGroupsIPH(true));
  base::Time show_time_max = base::Time::Now();

  FeaturePromoBubbleView* promo = promo_controller_->promo_bubble_for_testing();

  base::Time snooze_time_min = base::Time::Now();
  ClickButton(GetSnoozeButtonForTesting(promo));
  base::Time snooze_time_max = base::Time::Now();

  CheckSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                   /* is_dismiss */ false,
                   /* show_count */ 1,
                   /* snooze_count */ 1,
                   /* last_show_time_min */ show_time_min,
                   /* last_show_time_max */ show_time_max,
                   /* last_snooze_time_min */ snooze_time_min,
                   /* last_snooze_time_max */ snooze_time_max);
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest, CanReSnooze) {
  // Simulate the user snoozing the IPH.
  base::TimeDelta snooze_duration = base::TimeDelta::FromHours(26);
  base::Time snooze_time = base::Time::Now() - snooze_duration;
  base::Time show_time = snooze_time - base::TimeDelta::FromSeconds(1);
  SetSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                 /* is_dismiss */ false,
                 /* show_count */ 1,
                 /* snooze_count */ 1,
                 /* last_show_time */ show_time,
                 /* last_snooze_time */ snooze_time,
                 /* last_snooze_duration */ snooze_duration);

  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptTabGroupsIPH(true));
  base::Time show_time_max = base::Time::Now();

  FeaturePromoBubbleView* promo = promo_controller_->promo_bubble_for_testing();

  base::Time snooze_time_min = base::Time::Now();
  ClickButton(GetSnoozeButtonForTesting(promo));
  base::Time snooze_time_max = base::Time::Now();

  CheckSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                   /* is_dismiss */ false,
                   /* show_count */ 2,
                   /* snooze_count */ 2,
                   /* last_show_time_min */ show_time_min,
                   /* last_show_time_max */ show_time_max,
                   /* last_snooze_time_min */ snooze_time_min,
                   /* last_snooze_time_max */ snooze_time_max);
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DoesNotShowIfDismissed) {
  // Simulate the user dismissing the IPH.
  SetSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                 /* is_dismiss */ true,
                 /* show_count */ 1,
                 /* snooze_count */ 0,
                 /* last_show_time */ base::Time(),
                 /* last_snooze_time */ base::Time(),
                 /* last_snooze_duration */ base::TimeDelta());

  AttemptTabGroupsIPH(false);
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DoesNotShowBeforeSnoozeDuration) {
  // Simulate a very recent snooze.
  base::TimeDelta snooze_duration = base::TimeDelta::FromHours(26);
  base::Time snooze_time = base::Time::Now();
  base::Time show_time = snooze_time - base::TimeDelta::FromSeconds(1);
  SetSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                 /* is_dismiss */ false,
                 /* show_count */ 1,
                 /* snooze_count */ 1,
                 /* last_show_time */ show_time,
                 /* last_snooze_time */ snooze_time,
                 /* last_snooze_duration */ snooze_duration);

  AttemptTabGroupsIPH(false);
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       CloseBubbleSetsPrefs) {
  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptTabGroupsIPH(true));
  base::Time show_time_max = base::Time::Now();

  promo_controller_->CloseBubble(
      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);

  CheckSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                   /* is_dismiss */ false,
                   /* show_count */ 1,
                   /* snooze_count */ 0,
                   /* last_show_time_min */ show_time_min,
                   /* last_show_time_max */ show_time_max,
                   /* last_snooze_time_min */ base::Time(),
                   /* last_snooze_time_max */ base::Time());
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       WidgetCloseSetsPrefs) {
  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptTabGroupsIPH(true));
  base::Time show_time_max = base::Time::Now();

  FeaturePromoBubbleView* promo = promo_controller_->promo_bubble_for_testing();
  promo->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  CheckSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                   /* is_dismiss */ false,
                   /* show_count */ 1,
                   /* snooze_count */ 0,
                   /* last_show_time_min */ show_time_min,
                   /* last_show_time_max */ show_time_max,
                   /* last_snooze_time_min */ base::Time(),
                   /* last_snooze_time_max */ base::Time());
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       WorkWithoutNonClickerData) {
  // Non-clicker policy shipped pref entries that don't exist before.
  // Make sure empty entries are properly handled.
  base::TimeDelta snooze_duration = base::TimeDelta::FromHours(26);
  base::Time snooze_time = base::Time::Now() - snooze_duration;
  SetSnoozePrefs(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                 /* is_dismiss */ false,
                 /* show_count */ base::nullopt,
                 /* snooze_count */ 1,
                 /* last_show_time */ base::nullopt,
                 /* last_snooze_time */ snooze_time,
                 /* last_snooze_duration */ snooze_duration);

  AttemptTabGroupsIPH(true);
}
