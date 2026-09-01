// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_browsertest_base.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace user_education {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

using BubbleCloseCallback = FeaturePromoControllerImpl::BubbleCloseCallback;
using ShowPromoCallback = FeaturePromoControllerImpl::ShowPromoResultCallback;

using test::kCustomActionIPHFeature;
using test::kDefaultCustomActionIPHFeature;
using test::kPromoShownEvent;
using test::kSnoozeIPHFeature;
using test::kTestIPHFeature;
using test::kTestTutorialIdentifier;
using test::kTutorialIPHFeature;

BASE_FEATURE(kOneOffIPHFeature,
             "TEST_AnyContextIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionIPHFeature2,
             "TEST_CustomActionTestIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kStringTestIPHFeature,
             "TEST_StringTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOneOffIPHElementId);

class BrowserFeaturePromoControllerTest
    : public BrowserFeaturePromoControllerTestBase {
 public:
  enum class TrackerCallbackBehavior { kImmediate, kPost, kNever };

  BrowserFeaturePromoControllerTest() = default;
  ~BrowserFeaturePromoControllerTest() override = default;

  void SetUpOnMainThread() override {
    BrowserFeaturePromoControllerTestBase::SetUpOnMainThread();

    EXPECT_CALL(*mock_tracker_, IsInitialized).WillRepeatedly([this]() {
      return tracker_initialized_;
    });

    // Ensure that tests start after the grace period. The grace period itself
    // will be tested in the policy tests.
    ResetSessionDataImpl(kMoreThanGracePeriod, base::TimeDelta(),
                         BrowserView::GetBrowserViewForBrowser(browser()));
  }

  void SetTrackerInitBehavior(
      bool success,
      TrackerCallbackBehavior callback_behavior,
      base::OnceClosure additional_action = base::DoNothing()) {
    using OnInitializedCallback =
        feature_engagement::Tracker::OnInitializedCallback;
    tracker_initialized_ =
        callback_behavior == TrackerCallbackBehavior::kImmediate && success;
    auto wrapped_action = base::BindRepeating(
        [](base::OnceClosure& cb) {
          if (cb) {
            std::move(cb).Run();
          }
        },
        base::OwnedRef(std::move(additional_action)));
    EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
        .WillRepeatedly([this, success, callback_behavior, wrapped_action](
                            OnInitializedCallback on_initialized) mutable {
          tracker_initialized_ = success;
          switch (callback_behavior) {
            case TrackerCallbackBehavior::kImmediate:
              std::move(on_initialized).Run(success);
              wrapped_action.Run();
              break;
            case TrackerCallbackBehavior::kPost:
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      [](bool success, OnInitializedCallback cb,
                         base::RepeatingClosure wrapped_action) {
                        std::move(cb).Run(success);
                        wrapped_action.Run();
                      },
                      success, std::move(on_initialized), wrapped_action));
              break;
            case TrackerCallbackBehavior::kNever:
              wrapped_action.Run();
              break;
          }
        });
  }

  auto CheckNotShownMetrics(const base::Feature& feature,
                            FeaturePromoResult result,
                            int not_shown_count) {
    auto action_with_iph_name =
        base::StringPrintf("UserEducation.MessageNotShown.%s", feature.name);

    EXPECT_EQ(not_shown_count,
              user_action_tester_.GetActionCount(action_with_iph_name))
        << "Re: " << action_with_iph_name;

    auto failure = result.failure();
    if (!failure.has_value()) {
      return;
    }
    const std::string failure_action_name =
        "UserEducation.MessageNotShown." +
        FeaturePromoResult::GetFailureName(failure.value());
    EXPECT_EQ(not_shown_count,
              user_action_tester_.GetActionCount(failure_action_name))
        << "Re: " << failure_action_name;
  }

  void ExpectPromoResult(const base::Feature& feature,
                         FeaturePromoResult expected_result,
                         bool may_time_out,
                         BubbleCloseCallback close_callback = base::DoNothing(),
                         ShowPromoCallback show_callback = base::DoNothing()) {
    CHECK(!may_time_out || test_util_);
    FeaturePromoResult result;
    UNCALLED_MOCK_CALLBACK(ShowPromoCallback, mock_callback);
    ShowPromoCallback actual_show_callback = base::BindLambdaForTesting(
        [&result, callback = std::move(show_callback),
         mock = mock_callback.Get()](FeaturePromoResult actual_result) mutable {
          result = actual_result;
          std::move(callback).Run(actual_result);
          std::move(mock).Run(actual_result);
        });
    EXPECT_ASYNC_CALL_IN_SCOPE(mock_callback, Run, {
      controller_->MaybeShowPromo(MakeParams(feature, std::move(close_callback),
                                             std::move(actual_show_callback)),
                                  user_education_context_);
      if (may_time_out) {
        const auto new_now = test_util_->Now() +
                             features::GetLowPriorityTimeout() +
                             base::Seconds(1);
        test_util_->SetNow(new_now);
        test_util_->UpdateLastActiveTime(new_now, true);
      }
    });
    EXPECT_EQ(expected_result, result);
  }

  FeaturePromoParams MakeParams(
      const base::Feature& feature,
      FeaturePromoController::BubbleCloseCallback close_callback,
      FeaturePromoController::ShowPromoResultCallback startup_callback =
          base::NullCallback()) {
    FeaturePromoParams params(feature);
    params.close_callback = std::move(close_callback);
    params.show_promo_result_callback = std::move(startup_callback);
    return params;
  }

  HelpBubbleView* GetPromoBubble(HelpBubble* bubble) {
    if (!bubble) {
      return nullptr;
    }
    auto* const view =
        bubble->AsA<HelpBubbleViews>()->bubble_view_for_testing();
    return view ? views::AsViewClass<HelpBubbleView>(view) : nullptr;
  }

  HelpBubbleView* GetPromoBubble() {
    return GetPromoBubble(controller_->promo_bubble_for_testing());
  }

  void TimeOutQueuedPromo() {
    AdvanceTimeImpl(features::GetLowPriorityTimeout() + base::Seconds(1),
                    base::TimeDelta(), true);
  }

  Profile* profile() { return browser()->GetProfile(); }

 protected:
  base::UserActionTester user_action_tester_;
  bool tracker_initialized_ = true;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       NotifyFeatureUsedIfValidIsValid) {
  EXPECT_CALL(*mock_tracker_, NotifyUsedEvent(testing::Ref(kTestIPHFeature)))
      .Times(1);
  controller_->NotifyFeatureUsedIfValid(kTestIPHFeature);
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       GetFocusHelpBubbleScreenReaderHint) {
  EXPECT_TRUE(
      GetFocusHelpBubbleScreenReaderHint(
          FeaturePromoSpecification::PromoType::kToast, GetAnchorElement())
          .empty());
  EXPECT_FALSE(
      GetFocusHelpBubbleScreenReaderHint(
          FeaturePromoSpecification::PromoType::kSnooze, GetAnchorElement())
          .empty());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest, ShowsStartupBubble) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kImmediate);
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);

  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback, Run(FeaturePromoResult::Success()),
      controller_->MaybeShowStartupPromo(
          MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get()),
          user_education_context_));
  EXPECT_EQ(FeaturePromoStatus::kBubbleShowing,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       ShowStartupBubbleBlockedWithImmediateFailure) {
  SetTrackerInitBehavior(false, TrackerCallbackBehavior::kImmediate);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);
  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback, Run(FeaturePromoResult(FeaturePromoResult::kError)), {
        controller_->MaybeShowStartupPromo(
            MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get()),
            user_education_context_);
        TimeOutQueuedPromo();
      });
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       ShowStartupBubbleBlockedWithAsyncCallback) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, tracker_initialized);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);

  SetTrackerInitBehavior(false, TrackerCallbackBehavior::kPost,
                         tracker_initialized.Get());
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      tracker_initialized, Run, callback,
      Run(FeaturePromoResult(FeaturePromoResult::kError)), {
        controller_->MaybeShowStartupPromo(
            MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get()),
            user_education_context_);
        EXPECT_EQ(FeaturePromoStatus::kQueued,
                  controller_->GetPromoStatus(kTestIPHFeature));
        TimeOutQueuedPromo();
      });
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       ShowStartupBubbleWithAsyncCallback) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, tracker_initialized);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost,
                         tracker_initialized.Get());
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      tracker_initialized, Run, callback,
      Run(FeaturePromoResult(FeaturePromoResult::Success())), {
        controller_->MaybeShowStartupPromo(
            MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get()),
            user_education_context_);
        EXPECT_EQ(FeaturePromoStatus::kQueued,
                  controller_->GetPromoStatus(kTestIPHFeature));
      });
  EXPECT_EQ(FeaturePromoStatus::kBubbleShowing,
            controller_->GetPromoStatus(kTestIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       ShowStartupBubbleFailsWhenAlreadyShowing) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback2);

  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kImmediate);
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kSnoozeIPHFeature)))
      .WillOnce(Return(true));

  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback, Run,
      controller_->MaybeShowStartupPromo(
          MakeParams(kSnoozeIPHFeature, base::DoNothing(), callback.Get()),
          user_education_context_));
  EXPECT_TRUE(controller_->IsPromoActive(kSnoozeIPHFeature));
  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback2, Run(FeaturePromoResult(FeaturePromoResult::kAlreadyQueued)),
      controller_->MaybeShowStartupPromo(
          MakeParams(kSnoozeIPHFeature, base::DoNothing(), callback2.Get()),
          user_education_context_));
  EXPECT_TRUE(controller_->IsPromoActive(kSnoozeIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       ShowStartupBubbleFailsWhenAlreadyPending) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);

  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kNever);

  controller_->MaybeShowStartupPromo(kTestIPHFeature, user_education_context_);
  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback, Run(FeaturePromoResult(FeaturePromoResult::kAlreadyQueued)),
      controller_->MaybeShowStartupPromo(
          MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get()),
          user_education_context_));
  EXPECT_EQ(FeaturePromoStatus::kQueued,
            controller_->GetPromoStatus(kTestIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest,
                       CancelPromoBeforeStartup) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result_callback);

  tracker_initialized_ = false;
  feature_engagement::Tracker::OnInitializedCallback initialized_callback;
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([&](feature_engagement::Tracker::OnInitializedCallback cb) {
        initialized_callback = std::move(cb);
      });
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI).Times(0);

  controller_->MaybeShowStartupPromo(
      MakeParams(kTestIPHFeature, base::DoNothing(), result_callback.Get()),
      user_education_context_);
  EXPECT_EQ(FeaturePromoStatus::kQueued,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result_callback, Run(FeaturePromoResult(FeaturePromoResult::kCanceled)),
      controller_->EndPromo(kTestIPHFeature,
                            EndFeaturePromoReason::kAbortPromo));
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));

  // Now, indicate that startup has completed and verify that the promo does
  // not show.
  tracker_initialized_ = true;
  std::move(initialized_callback).Run(true);
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));
}

// Regression test for https://crbug.com/396344371
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTest, ShowPromoTwice) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kImmediate);

  bool first = true;
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillRepeatedly([&first]() {
        const bool result = first;
        first = false;
        return result;
      });

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback1);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback2);

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      callback1, Run(FeaturePromoResult::Success()), callback2,
      Run(testing::Ne(FeaturePromoResult::Success())), {
        controller_->MaybeShowStartupPromo(
            MakeParams(kTestIPHFeature, base::DoNothing(), callback1.Get()),
            user_education_context_);
        controller_->MaybeShowStartupPromo(
            MakeParams(kTestIPHFeature, base::DoNothing(), callback2.Get()),
            user_education_context_);
      });
  EXPECT_EQ(FeaturePromoStatus::kBubbleShowing,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

class BrowserFeaturePromoControllerTrackerInitializedTest
    : public BrowserFeaturePromoControllerTest {
 public:
  BrowserFeaturePromoControllerTrackerInitializedTest() = default;
  ~BrowserFeaturePromoControllerTrackerInitializedTest() override = default;

  void SetUpOnMainThread() override {
    BrowserFeaturePromoControllerTest::SetUpOnMainThread();
    SetTrackerInitBehavior(true, TrackerCallbackBehavior::kImmediate);
  }
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       FeatureEngagementTrackerEvents_DoNotBlockPromo) {
  feature_engagement::EventConfig config;
  config.name = "foo";
  config.comparator = feature_engagement::Comparator(
      feature_engagement::ComparatorType::LESS_THAN, 2);
  EXPECT_CALL(*mock_tracker_, ListEvents(testing::Ref(kTestIPHFeature)))
      .WillRepeatedly(
          Return(feature_engagement::Tracker::EventList{{config, 1}}));
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(testing::Ref(kTestIPHFeature)))
      .WillRepeatedly(Return(true));
  EXPECT_EQ(
      FeaturePromoResult::Success(),
      controller_->CanShowPromo(kTestIPHFeature, user_education_context_));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       FeatureEngagementTrackerEvents_DoBlockPromo) {
  feature_engagement::EventConfig config;
  config.name = "foo";
  config.comparator = feature_engagement::Comparator(
      feature_engagement::ComparatorType::LESS_THAN, 2);
  EXPECT_CALL(*mock_tracker_, ListEvents(testing::Ref(kTestIPHFeature)))
      .WillOnce(Return(feature_engagement::Tracker::EventList{{config, 2}}));
  EXPECT_EQ(
      FeaturePromoResult::kBlockedByConfig,
      controller_->CanShowPromo(kTestIPHFeature, user_education_context_));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       AsksBackendIfPromoShouldBeShown) {
  // If the backend says no, the controller says no.
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(false));
  EXPECT_EQ(
      FeaturePromoResult::kBlockedByConfig,
      controller_->CanShowPromo(kTestIPHFeature, user_education_context_));

  // If the backend says yes, the controller says yes.
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_EQ(
      FeaturePromoResult::Success(),
      controller_->CanShowPromo(kTestIPHFeature, user_education_context_));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       AsksBackendToShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(false));

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);

  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::kBlockedByConfig,
                    false, close_callback.Get());
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       DoesNotAskBackendWhenShowingFromDemoPage) {
  controller_->MaybeShowPromoForDemoPage(kTestIPHFeature,
                                         user_education_context_);
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_NE(nullptr, GetPromoBubble());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       ShowsBubble) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);
  CheckNotShownMetrics(kTestIPHFeature, FeaturePromoResult::Success(),
                       /*not_shown_count=*/0);
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       BubbleBlocksCanShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .WillRepeatedly(Return(true));
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);
  EXPECT_EQ(
      FeaturePromoResult::kBlockedByPromo,
      controller_->CanShowPromo(kTutorialIPHFeature, user_education_context_));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_TRUE(controller_->EndPromo(kTestIPHFeature,
                                    EndFeaturePromoReason::kFeatureEngaged));
  EXPECT_EQ(
      FeaturePromoResult::Success(),
      controller_->CanShowPromo(kTutorialIPHFeature, user_education_context_));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       ShowsBubbleAnyContext) {
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForTesting(
                    kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
                    .SetInAnyContext(true)));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kOneOffIPHFeature)))
      .WillOnce(Return(true));

  // Create a second widget with an element with the target identifier.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = browser_view()->GetWidget()->GetNativeWindow();
  widget->Init(std::move(params));
  widget->SetContentsView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  widget->Show();

  const ui::ElementContext widget_context =
      views::ElementTrackerViews::GetContextForWidget(widget.get());
  EXPECT_NE(browser_view()->GetElementContext(), widget_context);

  ExpectPromoResult(kOneOffIPHFeature, FeaturePromoResult::Success(), false);
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  EXPECT_EQ(widget_context,
            controller_->promo_bubble_for_testing()->GetContext());

  bubble->Close();
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       ShowsBubbleWithFilter) {
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForTesting(
                    kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
                    .SetAnchorElementFilter(base::BindLambdaForTesting(
                        [](const ui::ElementTracker::ElementList& elements) {
                          EXPECT_EQ(2U, elements.size());
                          return elements[0];
                        }))));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kOneOffIPHFeature)))
      .WillOnce(Return(true));

  // Add two random views to the browser with the same element ID.
  browser_view()
      ->toolbar()
      ->AddChildView(
          std::make_unique<views::StaticSizedView>(gfx::Size(10, 10)))
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  browser_view()
      ->toolbar()
      ->AddChildView(
          std::make_unique<views::StaticSizedView>(gfx::Size(10, 10)))
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);

  ExpectPromoResult(kOneOffIPHFeature, FeaturePromoResult::Success(), false);
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  bubble->Close();
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       ShowsBubbleWithFilterAnyContext) {
  ui::ElementContext widget_context;
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForTesting(
                    kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
                    .SetInAnyContext(true)
                    .SetAnchorElementFilter(base::BindLambdaForTesting(
                        [&](const ui::ElementTracker::ElementList& elements) {
                          EXPECT_EQ(3U, elements.size());
                          for (auto* element : elements) {
                            if (element->context() == widget_context) {
                              return element;
                            }
                          }
                          ADD_FAILURE() << "Did not find expected element.";
                          return (ui::TrackedElement*)(nullptr);
                        }))));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kOneOffIPHFeature)))
      .WillOnce(Return(true));

  // Add two random views to the browser with the same element ID.
  browser_view()
      ->toolbar()
      ->AddChildView(
          std::make_unique<views::StaticSizedView>(gfx::Size(10, 10)))
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  browser_view()
      ->toolbar()
      ->AddChildView(
          std::make_unique<views::StaticSizedView>(gfx::Size(10, 10)))
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);

  // Create a second widget with an element with the target identifier.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = browser_view()->GetWidget()->GetNativeWindow();
  widget->Init(std::move(params));
  widget->SetContentsView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  widget->Show();
  widget_context =
      views::ElementTrackerViews::GetContextForWidget(widget.get());

  EXPECT_NE(browser_view()->GetElementContext(), widget_context);

  ExpectPromoResult(kOneOffIPHFeature, FeaturePromoResult::Success(), false);
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  EXPECT_EQ(widget_context,
            controller_->promo_bubble_for_testing()->GetContext());

  bubble->Close();
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       DismissNonCriticalBubbleInRegion_RegionDoesNotOverlap) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

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

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       DismissNonCriticalBubbleInRegion_RegionOverlaps) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  const gfx::Rect bounds =
      GetPromoBubble()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_EQ(FeaturePromoResult::Success(), result);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       RequiredNoticeBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(0);

  auto& product_messaging_controller =
      UserEducationServiceFactory::GetForBrowserContext(profile())
          ->product_messaging_controller();

  DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(
      kRequiredNotice, ProductMessageType::kLegalOrComplianceNotice);

  ProductMessagingHandle handle_to_hold;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  product_messaging_controller.QueueMessage(
      kRequiredNotice,
      base::BindLambdaForTesting([&](ProductMessagingHandle handle) {
        handle_to_hold = std::move(handle);
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectPromoResult(kTutorialIPHFeature, FeaturePromoResult::kBlockedByPromo,
                    true);
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       NewProfileBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(0);
  // Simulate a new profile.
  storage_service()->set_profile_creation_time_for_testing(
      storage_service()->GetCurrentTime() - base::Hours(12));

  ExpectPromoResult(kTutorialIPHFeature,
                    FeaturePromoResult::kBlockedByNewProfile, false);
  CheckNotShownMetrics(kTutorialIPHFeature,
                       FeaturePromoResult::kBlockedByNewProfile,
                       /*not_shown_count=*/1);
  EXPECT_FALSE(controller_->IsPromoActive(kTutorialIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       SnoozeServiceBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(0);
  // Simulate a snooze by writing data directly.
  FeaturePromoData data;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_show_time = base::Time::Now();
  data.last_snooze_time = base::Time::Now();
  storage_service()->SavePromoData(kTutorialIPHFeature, data);

  ExpectPromoResult(kTutorialIPHFeature, FeaturePromoResult::kSnoozed, false);
  CheckNotShownMetrics(kTutorialIPHFeature, FeaturePromoResult::kSnoozed,
                       /*not_shown_count=*/1);
  EXPECT_FALSE(controller_->IsPromoActive(kTutorialIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
  storage_service()->Reset(kTutorialIPHFeature);
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PromoEndsWhenRequested) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false,
                    close_callback.Get());

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  EXPECT_CALL_IN_SCOPE(
      close_callback, Run(),
      EXPECT_TRUE(controller_->EndPromo(kTestIPHFeature,
                                        EndFeaturePromoReason::kAbortPromo)));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       CloseBubbleDoesNothingIfPromoNotShowing) {
  EXPECT_FALSE(controller_->EndPromo(kTestIPHFeature,
                                     EndFeaturePromoReason::kAbortPromo));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       CloseBubbleDoesNothingIfDifferentPromoShowing) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  EXPECT_FALSE(controller_->EndPromo(kTutorialIPHFeature,
                                     EndFeaturePromoReason::kAbortPromo));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PromoEndsOnBubbleClosure) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false,
                    close_callback.Get());

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  EXPECT_CALL_IN_SCOPE(close_callback, Run(), {
    bubble->GetWidget()->Close();
    widget_observer.Wait();
  });

  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       ContinuedPromoDefersBackendDismissed) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false,
                    close_callback.Get());

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  // First check that CloseBubbleAndContinuePromo() actually closes the
  // bubble, but doesn't yet tell the backend the promo finished.

  FeaturePromoHandle promo_handle;
  EXPECT_CALL_IN_SCOPE(
      close_callback, Run(),
      promo_handle = controller_->CloseBubbleAndContinuePromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoStatus::kContinued,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();

  // Check handle destruction causes the backend to be notified.

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       ContinuedPromoDismissesOnForceEnd) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  controller_->EndPromo(kTestIPHFeature, EndFeaturePromoReason::kAbortPromo);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature,
                                          FeaturePromoStatus::kContinued));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  promo_handle.Release();
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PromoHandleDismissesPromoOnRelease) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.
  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature,
                                          FeaturePromoStatus::kContinued));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PromoHandleDismissesPromoOnOverwrite) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle = FeaturePromoHandle();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PromoHandleDismissesPromoExactlyOnce) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  FeaturePromoHandle promo_handle =
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

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PromoHandleDismissesPromoAfterMoveConstruction) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoHandle promo_handle2(std::move(promo_handle));
  EXPECT_TRUE(promo_handle2);
  EXPECT_FALSE(promo_handle);  // NOLINT
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle2.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle2);
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PromoHandleDismissesPromoAfterMoveAssignment) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoHandle promo_handle2;
  promo_handle2 = std::move(promo_handle);
  EXPECT_TRUE(promo_handle2);
  EXPECT_FALSE(promo_handle);  // NOLINT
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle2.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle2);
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PropertySetOnAnchorViewWhileBubbleOpen) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  EXPECT_FALSE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));

  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::Success(), false);
  EXPECT_TRUE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));

  controller_->EndPromo(kTestIPHFeature, EndFeaturePromoReason::kAbortPromo);
  EXPECT_FALSE(GetAnchorView()->GetProperty(kHasInProductHelpPromoKey));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       FailsIfBubbleIsShowing) {
  HelpBubbleParams bubble_params;
  bubble_params.body_text = l10n_util::GetStringUTF16(IDS_CHROME_TIP);
  auto bubble = bubble_factory()->CreateHelpBubble(GetAnchorElement(),
                                                   std::move(bubble_params));
  EXPECT_TRUE(bubble);

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  ExpectPromoResult(kTestIPHFeature, FeaturePromoResult::kBlockedByPromo, true);
  CheckNotShownMetrics(kTestIPHFeature, FeaturePromoResult::kBlockedByPromo,
                       /*not_shown_count=*/1);
}

// Test that a feature promo can chain into a tutorial.
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       StartsTutorial) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kTutorialIPHFeature, FeaturePromoResult::Success(), false);

  // Simulate clicking the "Show Tutorial" button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  // We should be running the tutorial now.
  auto& tutorial_service = *UserEducationServiceFactory::GetForBrowserContext(
                                browser()->GetProfile())
                                ->tutorial_service();
  EXPECT_TRUE(tutorial_service.IsRunningTutorial());
  tutorial_service.CancelTutorialIfRunning();
}

// Test that a feature promo can perform a custom action.
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PerformsCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kCustomActionIPHFeature, FeaturePromoResult::Success(),
                    false);

  // Simulate clicking the custom action button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetNonDefaultButtonForTesting(0));
  waiter.Wait();

  EXPECT_EQ(1, custom_callback_count_);
}

// Test that a feature promo can perform a custom action that is the default.
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       PerformsCustomActionAsDefault) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kDefaultCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kDefaultCustomActionIPHFeature,
                    FeaturePromoResult::Success(), false);

  // Simulate clicking the custom action button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  auto* const button = bubble->GetNonDefaultButtonForTesting(0);
  ASSERT_TRUE(button);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOT_NOW), button->GetText());

  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  EXPECT_EQ(1, custom_callback_count_);
}

// Test that a feature promo does not perform a custom action when the default
// "Got it" button is clicked.
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       DoesNotPerformCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kCustomActionIPHFeature, FeaturePromoResult::Success(),
                    false);

  // Simulate clicking the other button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  EXPECT_EQ(0, custom_callback_count_);
}

// Test that a feature promo does not perform a custom action when a non-default
// "Got it" button is clicked.
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       DoesNotPerformDefaultCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kDefaultCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ExpectPromoResult(kDefaultCustomActionIPHFeature,
                    FeaturePromoResult::Success(), false);

  // Simulate clicking the other button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetNonDefaultButtonForTesting(0));
  waiter.Wait();

  EXPECT_EQ(0, custom_callback_count_);
}

// Test that the promo controller can handle the anchor view disappearing from
// under the bubble during the button callback.
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerTrackerInitializedTest,
                       CustomActionHidesAnchorView) {
  FeaturePromoHandle promo_handle;
  registry()->RegisterFeature(FeaturePromoSpecification::CreateForCustomAction(
      kCustomActionIPHFeature2, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP,
      IDS_CHROME_TIP,
      base::BindLambdaForTesting([&](const UserEducationContextPtr& context,
                                     FeaturePromoHandle handle) {
        views::ElementTrackerViews::GetInstance()
            ->GetUniqueView(kToolbarAppMenuButtonElementId,
                            context->GetElementContext())
            ->SetVisible(false);
        promo_handle = std::move(handle);
      })));

  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature2)))
      .WillOnce(Return(true));
  ExpectPromoResult(kCustomActionIPHFeature2, FeaturePromoResult::Success(),
                    false);

  // Simulate clicking the custom action button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetNonDefaultButtonForTesting(0));
  waiter.Wait();
  EXPECT_TRUE(promo_handle.is_valid());

  // Promo is actually dismissed when the handle is released.
  EXPECT_CALL(*mock_tracker_, Dismissed(testing::Ref(kCustomActionIPHFeature2)))
      .Times(1);
  promo_handle.Release();
}

constexpr int kStringWithNoSubstitution = IDS_OK;

class BrowserFeaturePromoControllerViewsTestBase
    : public BrowserFeaturePromoControllerTestBase {
 public:
  enum class TrackerCallbackBehavior { kImmediate, kPost, kNever };

  BrowserFeaturePromoControllerViewsTestBase() = default;
  ~BrowserFeaturePromoControllerViewsTestBase() override = default;

  void SetTrackerInitBehavior(
      bool success,
      TrackerCallbackBehavior callback_behavior,
      base::OnceClosure additional_action = base::DoNothing()) {
    using OnInitializedCallback =
        feature_engagement::Tracker::OnInitializedCallback;
    tracker_initialized_ =
        callback_behavior == TrackerCallbackBehavior::kImmediate && success;
    auto wrapped_action = base::BindRepeating(
        [](base::OnceClosure& cb) {
          if (cb) {
            std::move(cb).Run();
          }
        },
        base::OwnedRef(std::move(additional_action)));
    EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
        .WillRepeatedly([this, success, callback_behavior, wrapped_action](
                            OnInitializedCallback on_initialized) mutable {
          tracker_initialized_ = success;
          switch (callback_behavior) {
            case TrackerCallbackBehavior::kImmediate:
              std::move(on_initialized).Run(success);
              wrapped_action.Run();
              break;
            case TrackerCallbackBehavior::kPost:
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      [](bool success, OnInitializedCallback cb,
                         base::RepeatingClosure wrapped_action) {
                        std::move(cb).Run(success);
                        wrapped_action.Run();
                      },
                      success, std::move(on_initialized), wrapped_action));
              break;
            case TrackerCallbackBehavior::kNever:
              wrapped_action.Run();
              break;
          }
        });
  }

  auto RegisterPromo(int body_string, int title_string = 0) {
    return Do([this, body_string, title_string]() {
      auto spec = FeaturePromoSpecification::CreateForTesting(
          kStringTestIPHFeature, kToolbarAppMenuButtonElementId, body_string);
      if (title_string) {
        spec.SetBubbleTitleText(title_string);
      }
      registry()->RegisterFeature(std::move(spec));
    });
  }

  auto RegisterAccessiblePromo(
      int screenreader_string,
      FeaturePromoSpecification::AcceleratorInfo accelerator =
          FeaturePromoSpecification::AcceleratorInfo()) {
    return Do([this, screenreader_string, accelerator]() {
      auto spec = FeaturePromoSpecification::CreateForToastPromo(
          kStringTestIPHFeature, kToolbarAppMenuButtonElementId,
          kStringWithNoSubstitution, screenreader_string, accelerator);
      registry()->RegisterFeature(std::move(spec));
    });
  }

  auto CheckAccessibleText(std::u16string expected_text) {
    return CheckView(
        HelpBubbleView::kHelpBubbleElementIdForTesting,
        [](HelpBubbleView* bubble) {
          return static_cast<views::BubbleDialogDelegate*>(bubble)
              ->GetAccessibleWindowTitle();
        },
        expected_text);
  }

 private:
  bool tracker_initialized_ = true;
};

class BrowserFeaturePromoControllerViewsTest
    : public BrowserFeaturePromoControllerViewsTestBase {
 public:
  BrowserFeaturePromoControllerViewsTest() = default;
  ~BrowserFeaturePromoControllerViewsTest() override = default;

  void SetUpOnMainThread() override {
    BrowserFeaturePromoControllerViewsTestBase::SetUpOnMainThread();
    SetTrackerInitBehavior(true, TrackerCallbackBehavior::kImmediate);
  }
};

// In branded builds on Windows, some of the required strings may be optimized
// out during Chrome resource compilation. To avoid issues, simply don't run
// these tests on those specific bots.
// See https://crbug.com/434261108 and https://crbug.com/40750695 for more info.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) || !BUILDFLAG(IS_WIN)

constexpr int kStringWithSingleSubstitution =
    IDS_APP_TABLE_COLUMN_SORTED_ASC_ACCNAME;
constexpr int kStringWithMultipleSubstitutions =
    IDS_CONCAT_THREE_STRINGS_WITH_COMMA;
constexpr int kStringWithPluralSubstitution = IDS_TIME_HOURS;
inline constexpr char16_t kSubstitution1[] = u"First";
inline constexpr char16_t kSubstitution2[] = u"Second";
inline constexpr char16_t kSubstitution3[] = u"Third";

using BrowserFeaturePromoControllerStringSubstitutionTest =
    BrowserFeaturePromoControllerViewsTest;

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       BodyTextSubstitution_SingleString) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = kSubstitution1;

  RunTestSequence(
      RegisterPromo(kStringWithSingleSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       BodyTextSubstitution_MultipleStrings) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = FeaturePromoSpecification::StringSubstitutions{
      kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(
      RegisterPromo(kStringWithMultipleSubstitutions),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          HelpBubbleView::kBodyTextIdForTesting, &views::Label::GetText,
          l10n_util::GetStringFUTF16(kStringWithMultipleSubstitutions,
                                     kSubstitution1, kSubstitution2,
                                     kSubstitution3)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       BodyTextSubstitution_Singular) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = 1;

  RunTestSequence(
      RegisterPromo(kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          HelpBubbleView::kBodyTextIdForTesting, &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       BodyTextSubstitution_Plural) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = 3;

  RunTestSequence(
      RegisterPromo(kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          HelpBubbleView::kBodyTextIdForTesting, &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       TitleTextSubstitution_SingleString) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = kSubstitution1;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithSingleSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       TitleTextSubstitution_MultipleStrings) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = FeaturePromoSpecification::StringSubstitutions{
      kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithMultipleSubstitutions),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          HelpBubbleView::kTitleTextIdForTesting, &views::Label::GetText,
          l10n_util::GetStringFUTF16(kStringWithMultipleSubstitutions,
                                     kSubstitution1, kSubstitution2,
                                     kSubstitution3)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       TitleTextSubstitution_Singular) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = 1;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          HelpBubbleView::kTitleTextIdForTesting, &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       TitleTextSubstitution_Plural) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = 3;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          HelpBubbleView::kTitleTextIdForTesting, &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       ScreenreaderTextSubstitution_Accelerator) {
  static const ui::Accelerator kAccelerator(ui::VKEY_ESCAPE, ui::EF_NONE);
  FeaturePromoParams params(kStringTestIPHFeature);

  RunTestSequence(
      RegisterAccessiblePromo(
          kStringWithSingleSubstitution,
          FeaturePromoSpecification::AcceleratorInfo(kAccelerator)),
      MaybeShowPromo(std::move(params)),
      CheckAccessibleText(l10n_util::GetStringFUTF16(
          kStringWithSingleSubstitution, kAccelerator.GetShortcutText())));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       ScreenreaderTextSubstitution_SingleString) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params = kSubstitution1;

  RunTestSequence(RegisterAccessiblePromo(kStringWithSingleSubstitution),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetStringFUTF16(
                      kStringWithSingleSubstitution, kSubstitution1)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       ScreenreaderTextSubstitution_MultipleStrings) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params = FeaturePromoSpecification::StringSubstitutions{
      kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(RegisterAccessiblePromo(kStringWithMultipleSubstitutions),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetStringFUTF16(
                      kStringWithMultipleSubstitutions, kSubstitution1,
                      kSubstitution2, kSubstitution3)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       ScreenreaderTextSubstitution_Singular) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params = 1;

  RunTestSequence(RegisterAccessiblePromo(kStringWithPluralSubstitution),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetPluralStringFUTF16(
                      kStringWithPluralSubstitution, 1)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerStringSubstitutionTest,
                       ScreenreaderTextSubstitution_Plural) {
  FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params = 3;

  RunTestSequence(RegisterAccessiblePromo(kStringWithPluralSubstitution),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetPluralStringFUTF16(
                      kStringWithPluralSubstitution, 3)));
}

#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING) || !BUILDFLAG(IS_WIN)

}  // namespace

}  // namespace user_education
