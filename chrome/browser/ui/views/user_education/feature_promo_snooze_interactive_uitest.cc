// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
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

class FeaturePromoSnoozeInteractiveTest : public InProcessBrowserTest {
 public:
  FeaturePromoSnoozeInteractiveTest() {
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
                            ->GetFeaturePromoController();
    snooze_service_ = promo_controller_->snooze_service();

    if (!promo_controller_->registry()->IsFeatureRegistered(
            kSnoozeTestFeature)) {
      promo_controller_->registry()->RegisterFeature(
          user_education::FeaturePromoSpecification::CreateForSnoozePromo(
              kSnoozeTestFeature, kAppMenuButtonElementId,
              IDS_TAB_GROUPS_NEW_GROUP_PROMO));
    }
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
                      absl::optional<int> show_count,
                      int snooze_count,
                      absl::optional<base::Time> last_show_time,
                      base::Time last_snooze_time,
                      base::TimeDelta last_snooze_duration) {
    user_education::FeaturePromoSnoozeService::SnoozeData data;
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
  void AttemptIPH(bool should_show) {
    if (should_show) {
      EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kSnoozeTestFeature)))
          .WillOnce(Return(true));
    } else {
      EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kSnoozeTestFeature)))
          .Times(0);
    }

    ASSERT_EQ(should_show,
              promo_controller_->MaybeShowPromo(kSnoozeTestFeature));
    ASSERT_EQ(should_show,
              promo_controller_->IsPromoActive(kSnoozeTestFeature));

    // If shown, Tracker::Dismissed should be called eventually.
    if (should_show) {
      EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kSnoozeTestFeature)));
    }
  }

  user_education::HelpBubbleView* GetPromoBubbleView() {
    return promo_controller_->promo_bubble_for_testing()
        ->AsA<user_education::HelpBubbleViews>()
        ->bubble_view();
  }

  views::Button* GetSnoozeButtonForTesting() {
    return GetPromoBubbleView()->GetNonDefaultButtonForTesting(0);
  }

  views::Button* GetDismissButtonForTesting() {
    return GetPromoBubbleView()->GetDefaultButtonForTesting();
  }

  raw_ptr<NiceMock<feature_engagement::test::MockTracker>, DanglingUntriaged>
      mock_tracker_;
  raw_ptr<BrowserFeaturePromoController, DanglingUntriaged> promo_controller_;
  raw_ptr<user_education::FeaturePromoSnoozeService, DanglingUntriaged>
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

  base::test::ScopedFeatureList scoped_feature_list_;

  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DismissDoesNotSnooze) {
  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptIPH(true));
  base::Time show_time_max = base::Time::Now();

  ClickButton(GetDismissButtonForTesting());
  CheckSnoozePrefs(kSnoozeTestFeature,
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
  ASSERT_NO_FATAL_FAILURE(AttemptIPH(true));
  base::Time show_time_max = base::Time::Now();

  base::Time snooze_time_min = base::Time::Now();
  ClickButton(GetSnoozeButtonForTesting());
  base::Time snooze_time_max = base::Time::Now();

  CheckSnoozePrefs(kSnoozeTestFeature,
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
  base::TimeDelta snooze_duration = base::Hours(26);
  base::Time snooze_time = base::Time::Now() - snooze_duration;
  base::Time show_time = snooze_time - base::Seconds(1);
  SetSnoozePrefs(kSnoozeTestFeature,
                 /* is_dismiss */ false,
                 /* show_count */ 1,
                 /* snooze_count */ 1,
                 /* last_show_time */ show_time,
                 /* last_snooze_time */ snooze_time,
                 /* last_snooze_duration */ snooze_duration);

  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptIPH(true));
  base::Time show_time_max = base::Time::Now();

  base::Time snooze_time_min = base::Time::Now();
  ClickButton(GetSnoozeButtonForTesting());
  base::Time snooze_time_max = base::Time::Now();

  CheckSnoozePrefs(kSnoozeTestFeature,
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
  SetSnoozePrefs(kSnoozeTestFeature,
                 /* is_dismiss */ true,
                 /* show_count */ 1,
                 /* snooze_count */ 0,
                 /* last_show_time */ base::Time(),
                 /* last_snooze_time */ base::Time(),
                 /* last_snooze_duration */ base::TimeDelta());

  AttemptIPH(false);
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest,
                       DoesNotShowBeforeSnoozeDuration) {
  // Simulate a very recent snooze.
  base::TimeDelta snooze_duration = base::Hours(26);
  base::Time snooze_time = base::Time::Now();
  base::Time show_time = snooze_time - base::Seconds(1);
  SetSnoozePrefs(kSnoozeTestFeature,
                 /* is_dismiss */ false,
                 /* show_count */ 1,
                 /* snooze_count */ 1,
                 /* last_show_time */ show_time,
                 /* last_snooze_time */ snooze_time,
                 /* last_snooze_duration */ snooze_duration);

  AttemptIPH(false);
}

IN_PROC_BROWSER_TEST_F(FeaturePromoSnoozeInteractiveTest, EndPromoSetsPrefs) {
  base::Time show_time_min = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(AttemptIPH(true));
  base::Time show_time_max = base::Time::Now();

  promo_controller_->EndPromo(kSnoozeTestFeature);

  CheckSnoozePrefs(kSnoozeTestFeature,
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
  ASSERT_NO_FATAL_FAILURE(AttemptIPH(true));
  base::Time show_time_max = base::Time::Now();

  auto* const bubble = GetPromoBubbleView();
  bubble->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  CheckSnoozePrefs(kSnoozeTestFeature,
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
  base::TimeDelta snooze_duration = base::Hours(26);
  base::Time snooze_time = base::Time::Now() - snooze_duration;
  SetSnoozePrefs(kSnoozeTestFeature,
                 /* is_dismiss */ false,
                 /* show_count */ absl::nullopt,
                 /* snooze_count */ 1,
                 /* last_show_time */ absl::nullopt,
                 /* last_snooze_time */ snooze_time,
                 /* last_snooze_duration */ snooze_duration);

  AttemptIPH(true);
}
