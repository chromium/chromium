// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/test/feature_promo_session_test_util.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

namespace {
BASE_FEATURE(kTestIPHFeature,
             "TEST_TestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOneOffIPHFeature,
             "TEST_AnyContextIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSnoozeIPHFeature,
             "TEST_SnoozeIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTutorialIPHFeature,
             "TEST_TutorialTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionIPHFeature,
             "TEST_CustomActionTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultCustomActionIPHFeature,
             "TEST_DefaultCustomActionTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionIPHFeature2,
             "TEST_CustomActionTestIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kStringTestIPHFeature,
             "TEST_StringTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr char kTestTutorialIdentifier[] = "Test Tutorial";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOneOffIPHElementId);
}  // namespace

using user_education::FeaturePromoClosedReason;
using user_education::FeaturePromoController;
using user_education::FeaturePromoData;
using user_education::FeaturePromoHandle;
using user_education::FeaturePromoPolicyData;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoResult;
using user_education::FeaturePromoSessionData;
using user_education::FeaturePromoSpecification;
using user_education::FeaturePromoStatus;
using user_education::FeaturePromoStorageService;
using user_education::HelpBubble;
using user_education::HelpBubbleArrow;
using user_education::HelpBubbleFactoryRegistry;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;
using user_education::HelpBubbleViews;
using user_education::TutorialDescription;

using BubbleCloseCallback = BrowserFeaturePromoController::BubbleCloseCallback;
using ShowPromoCallback =
    BrowserFeaturePromoController::ShowPromoResultCallback;

class BrowserFeaturePromoControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Disable all registered IPH. These tests use only test features.
    for (const auto& feature : feature_engagement::GetAllFeatures()) {
      disabled_features.emplace_back(*feature);
    }

    // Enable or disable V2.
    if (UseV2()) {
      enabled_features.emplace_back(
          user_education::features::kUserEducationExperienceVersion2);
    } else {
      disabled_features.emplace_back(
          user_education::features::kUserEducationExperienceVersion2);
    }

    // Do the enabling or disabling.
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    TestWithBrowserView::SetUp();
    controller_ = static_cast<BrowserFeaturePromoController*>(
        browser_view()->GetFeaturePromoControllerForTesting());
    lock_ = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                profile()));
    EXPECT_CALL(*mock_tracker_, IsInitialized).WillRepeatedly([this]() {
      return tracker_initialized_;
    });

    registry()->clear_features_for_testing();

    // Register placeholder tutorials and IPH journeys.

    auto* const user_education_service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());

    // Ensure that the new profile grace period has ended by default.
    auto& storage_service =
        user_education_service->feature_promo_storage_service();
    storage_service.set_profile_creation_time_for_testing(
        storage_service.GetCurrentTime() - base::Days(365));

    // Create a dummy tutorial.
    // This is just the first two steps of the "create tab group" tutorial.
    TutorialDescription desc;
    desc.steps.emplace_back(
        TutorialDescription::BubbleStep(kTabStripElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kTopCenter));
    desc.steps.emplace_back(
        TutorialDescription::BubbleStep(kTabGroupEditorBubbleId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
            .AbortIfVisibilityLost(false));

    user_education_service->tutorial_registry().AddTutorial(
        kTestTutorialIdentifier, std::move(desc));

    RegisterIPH();

    // Make sure the browser view is visible for the tests.
    browser_view()->GetWidget()->Show();
  }

  virtual void RegisterIPH() {
    registry()->RegisterFeature(DefaultPromoSpecification());

    registry()->RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
        kSnoozeIPHFeature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP));

    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateForTutorialPromo(
            kTutorialIPHFeature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP,
            kTestTutorialIdentifier));

    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateForCustomAction(
            kCustomActionIPHFeature, kToolbarAppMenuButtonElementId,
            IDS_CHROME_TIP, IDS_CHROME_TIP,
            base::BindRepeating(
                &BrowserFeaturePromoControllerTest::OnCustomPromoAction,
                base::Unretained(this),
                base::Unretained(&kCustomActionIPHFeature))));

    auto default_custom = FeaturePromoSpecification::CreateForCustomAction(
        kDefaultCustomActionIPHFeature, kToolbarAppMenuButtonElementId,
        IDS_CHROME_TIP, IDS_CHROME_TIP,
        base::BindRepeating(
            &BrowserFeaturePromoControllerTest::OnCustomPromoAction,
            base::Unretained(this),
            base::Unretained(&kDefaultCustomActionIPHFeature)));
    default_custom.SetCustomActionIsDefault(true);
    default_custom.SetCustomActionDismissText(IDS_NOT_NOW);
    registry()->RegisterFeature(std::move(default_custom));
  }

  void TearDown() override {
    TestWithBrowserView::TearDown();
    lock_.reset();
  }

  enum class TrackerCallbackBehavior { kImmediate, kPost, kNever };

  void SetTrackerInitBehavior(
      bool success,
      TrackerCallbackBehavior callback_behavior,
      base::OnceClosure additional_action = base::DoNothing()) {
    using OnInitializedCallback =
        feature_engagement::Tracker::OnInitializedCallback;
    tracker_initialized_ = false;
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

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(
            BrowserFeaturePromoControllerTest::MakeTestTracker));
    return factories;
  }

  auto CheckNotShownMetrics(const base::Feature& feature,
                            FeaturePromoResult result,
                            int not_shown_count) {
    // Can't check the general "not shown" count since some other startup promos
    // might have been blocked (this is a live browser window).

    auto action_with_iph_name =
        base::StringPrintf("UserEducation.MessageNotShown.%s", feature.name);

    EXPECT_EQ(not_shown_count,
              user_action_tester_.GetActionCount(action_with_iph_name));

    auto failure = result.failure();
    if (!failure.has_value()) {
      return;
    }
    std::string failure_action_name = "UserEducation.MessageNotShown.";
    switch (failure.value()) {
      case FeaturePromoResult::kCanceled:
        failure_action_name.append("Canceled");
        break;
      case FeaturePromoResult::kError:
        failure_action_name.append("Error");
        break;
      case FeaturePromoResult::kBlockedByUi:
        failure_action_name.append("BlockedByUi");
        break;
      case FeaturePromoResult::kBlockedByPromo:
        failure_action_name.append("BlockedByPromo");
        break;
      case FeaturePromoResult::kBlockedByConfig:
        failure_action_name.append("BlockedByConfig");
        break;
      case FeaturePromoResult::kSnoozed:
        failure_action_name.append("Snoozed");
        break;
      case FeaturePromoResult::kBlockedByContext:
        failure_action_name.append("BlockedByContext");
        break;
      case FeaturePromoResult::kFeatureDisabled:
        failure_action_name.append("FeatureDisabled");
        break;
      case FeaturePromoResult::kPermanentlyDismissed:
        failure_action_name.append("PermanentlyDismissed");
        break;
      case FeaturePromoResult::kBlockedByGracePeriod:
        failure_action_name.append("BlockedByGracePeriod");
        break;
      case FeaturePromoResult::kBlockedByCooldown:
        failure_action_name.append("BlockedByCooldown");
        break;
      case FeaturePromoResult::kRecentlyAborted:
        failure_action_name.append("RecentlyAborted");
        break;
      case FeaturePromoResult::kExceededMaxShowCount:
        failure_action_name.append("ExceededMaxShowCount");
        break;
      case FeaturePromoResult::kBlockedByNewProfile:
        failure_action_name.append("BlockedByNewProfile");
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    EXPECT_EQ(not_shown_count,
              user_action_tester_.GetActionCount(failure_action_name));
  }

  FeaturePromoResult GetPromoResult(
      const base::Feature& feature,
      BubbleCloseCallback close_callback = base::DoNothing(),
      ShowPromoCallback show_callback = base::DoNothing()) {
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
                                             std::move(actual_show_callback)));
    });
    return result;
  }

 protected:
  FeaturePromoController* controller() { return controller_.get(); }

  virtual bool UseV2() const { return false; }

  user_education::FeaturePromoParams MakeParams(
      const base::Feature& feature,
      user_education::FeaturePromoController::BubbleCloseCallback
          close_callback,
      user_education::FeaturePromoController::ShowPromoResultCallback
          startup_callback = base::NullCallback()) {
    user_education::FeaturePromoParams params(feature);
    params.close_callback = std::move(close_callback);
    params.show_promo_result_callback = std::move(startup_callback);
    return params;
  }

  FeaturePromoStorageService* storage_service() {
    return controller_->storage_service();
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

  FeaturePromoSpecification DefaultPromoSpecification(
      const base::Feature& feature = kTestIPHFeature) {
    return FeaturePromoSpecification::CreateForToastPromo(
        feature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP, IDS_OK,
        FeaturePromoSpecification::AcceleratorInfo());
  }

  void OnCustomPromoAction(const base::Feature* feature,
                           ui::ElementContext context,
                           FeaturePromoHandle promo_handle) {
    ++custom_callback_count_;
    EXPECT_TRUE(promo_handle.is_valid());
    EXPECT_EQ(FeaturePromoStatus::kContinued,
              controller_->GetPromoStatus(*feature));
    EXPECT_EQ(browser()->window()->GetElementContext(), context);
    promo_handle.Release();
    EXPECT_EQ(FeaturePromoStatus::kNotRunning,
              controller_->GetPromoStatus(*feature));
  }

  raw_ptr<BrowserFeaturePromoController, DanglingUntriaged> controller_;
  raw_ptr<NiceMock<feature_engagement::test::MockTracker>, DanglingUntriaged>
      mock_tracker_;
  BrowserFeaturePromoController::TestLock lock_;
  int custom_callback_count_ = 0;
  bool tracker_initialized_ = true;

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
    EXPECT_CALL(*tracker, ListEvents)
        .WillRepeatedly(Return(feature_engagement::Tracker::EventList()));

    return tracker;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::UserActionTester user_action_tester_;
};

using BubbleCloseCallback = BrowserFeaturePromoController::BubbleCloseCallback;

TEST_F(BrowserFeaturePromoControllerTest, NotifyFeatureUsedIfValidIsValid) {
  EXPECT_CALL(*mock_tracker_, NotifyUsedEvent(testing::Ref(kTestIPHFeature)))
      .Times(1);
  controller_->NotifyFeatureUsedIfValid(kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoControllerTest,
       FeatureEngagementTrackerEvents_DoNotBlockPromo) {
  feature_engagement::EventConfig config;
  config.name = "foo";
  config.comparator = feature_engagement::Comparator(
      feature_engagement::ComparatorType::LESS_THAN, 2);
  EXPECT_CALL(*mock_tracker_, ListEvents(testing::Ref(kTestIPHFeature)))
      .WillOnce(Return(feature_engagement::Tracker::EventList{{config, 1}}));
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(testing::Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_EQ(FeaturePromoResult::Success(),
            controller_->CanShowPromo(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       FeatureEngagementTrackerEvents_DoBlockPromo) {
  feature_engagement::EventConfig config;
  config.name = "foo";
  config.comparator = feature_engagement::Comparator(
      feature_engagement::ComparatorType::LESS_THAN, 2);
  EXPECT_CALL(*mock_tracker_, ListEvents(testing::Ref(kTestIPHFeature)))
      .WillOnce(Return(feature_engagement::Tracker::EventList{{config, 2}}));
  EXPECT_EQ(FeaturePromoResult::kBlockedByConfig,
            controller_->CanShowPromo(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendIfPromoShouldBeShown) {
  // If the backend says no, the controller says no.
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(false));
  EXPECT_EQ(FeaturePromoResult::kBlockedByConfig,
            controller_->CanShowPromo(kTestIPHFeature));

  // If the backend says yes, the controller says yes.
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->CanShowPromo(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendToShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(false));

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);

  EXPECT_FALSE(GetPromoResult(kTestIPHFeature, close_callback.Get()));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendToShowStartupPromo) {
  SetTrackerInitBehavior(false, TrackerCallbackBehavior::kImmediate);

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);
  EXPECT_CALL_IN_SCOPE(
      callback, Run(FeaturePromoResult(FeaturePromoResult::kError)),
      controller_->MaybeShowStartupPromo(
          MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get())));
}

TEST_F(BrowserFeaturePromoControllerTest,
       DoesNotAskBackendWhenShowingFromDemoPage) {
  EXPECT_TRUE(controller_->MaybeShowPromoForDemoPage(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_NE(nullptr, GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubble) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  const auto result = GetPromoResult(kTestIPHFeature);
  EXPECT_TRUE(result);
  CheckNotShownMetrics(kTestIPHFeature, result, /*not_shown_count=*/0);
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, BubbleBlocksCanShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(GetPromoResult(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoResult::kBlockedByPromo,
            controller_->CanShowPromo(kTutorialIPHFeature));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_TRUE(controller_->EndPromo(
      kTestIPHFeature, user_education::EndFeaturePromoReason::kFeatureEngaged));
  EXPECT_TRUE(controller_->CanShowPromo(kTutorialIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsStartupBubble) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kImmediate);
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);

  EXPECT_CALL_IN_SCOPE(
      callback, Run(FeaturePromoResult::Success()),
      EXPECT_TRUE(controller_->MaybeShowStartupPromo(
          MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get()))));
  EXPECT_EQ(FeaturePromoStatus::kBubbleShowing,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowStartupBlockedWithAsyncCallback) {
  base::RunLoop run_loop;
  SetTrackerInitBehavior(false, TrackerCallbackBehavior::kPost,
                         run_loop.QuitClosure());

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(
      MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get())));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_CALL_IN_SCOPE(callback,
                       Run(FeaturePromoResult(FeaturePromoResult::kError)),
                       run_loop.Run());
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, ShowStartupBubbleWithAsyncCallback) {
  base::RunLoop run_loop;
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost,
                         run_loop.QuitClosure());
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         callback);

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(
      MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get())));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_CALL_IN_SCOPE(callback, Run(FeaturePromoResult::Success()),
                       run_loop.Run());
  EXPECT_EQ(FeaturePromoStatus::kBubbleShowing,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       ShowStartupBubbleFailsWhenAlreadyShowing) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kImmediate);
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       ShowStartupBubbleFailsWhenAlreadyPending) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kNever);

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, CancelPromoBeforeStartup) {
  tracker_initialized_ = false;
  feature_engagement::Tracker::OnInitializedCallback callback;
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([&](feature_engagement::Tracker::OnInitializedCallback cb) {
        callback = std::move(cb);
      });
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI).Times(0);

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  controller_->EndPromo(kTestIPHFeature,
                        user_education::EndFeaturePromoReason::kAbortPromo);
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));

  // Now, indicate that startup has completed and verify that the promo does
  // not show.
  tracker_initialized_ = true;
  std::move(callback).Run(true);
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubbleAnyContext) {
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
                    .SetInAnyContext(true)));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kOneOffIPHFeature)))
      .WillOnce(Return(true));

  // Create a second widget with an element with the target identifier.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
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

  EXPECT_TRUE(GetPromoResult(kOneOffIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  EXPECT_EQ(widget_context,
            controller_->promo_bubble_for_testing()->GetContext());

  bubble->Close();
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubbleWithFilter) {
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
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
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  browser_view()
      ->toolbar()
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);

  EXPECT_TRUE(GetPromoResult(kOneOffIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  bubble->Close();
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubbleWithFilterAnyContext) {
  ui::ElementContext widget_context;
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
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
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  browser_view()
      ->toolbar()
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);

  // Create a second widget with an element with the target identifier.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
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

  EXPECT_TRUE(GetPromoResult(kOneOffIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  EXPECT_EQ(widget_context,
            controller_->promo_bubble_for_testing()->GetContext());

  bubble->Close();
}

TEST_F(BrowserFeaturePromoControllerTest,
       DismissNonCriticalBubbleInRegion_RegionDoesNotOverlap) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_TRUE(GetPromoResult(kTestIPHFeature));

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
      .WillOnce(Return(true));
  EXPECT_TRUE(GetPromoResult(kTestIPHFeature));

  const gfx::Rect bounds =
      GetPromoBubble()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_TRUE(result);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, RequiredNoticeBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(0);

  auto& product_messaging_controller =
      UserEducationServiceFactory::GetForBrowserContext(profile())
          ->product_messaging_controller();

  DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kRequiredNotice);

  user_education::RequiredNoticePriorityHandle handle_to_hold;
  base::RunLoop run_loop;
  product_messaging_controller.QueueRequiredNotice(
      kRequiredNotice,
      base::BindLambdaForTesting(
          [&](user_education::RequiredNoticePriorityHandle handle) {
            handle_to_hold = std::move(handle);
            run_loop.Quit();
          }));

  EXPECT_EQ(user_education::FeaturePromoResult::kBlockedByPromo,
            GetPromoResult(kTutorialIPHFeature));

  // Run the loop, ensuring that the notice is cleared.
  run_loop.Run();
}

TEST_F(BrowserFeaturePromoControllerTest, NewProfileBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(0);
  // Simulate a new profile.
  storage_service()->set_profile_creation_time_for_testing(
      storage_service()->GetCurrentTime() - base::Hours(12));

  auto result = GetPromoResult(kTutorialIPHFeature);
  EXPECT_FALSE(result);
  CheckNotShownMetrics(kTutorialIPHFeature, result, /*not_shown_count=*/1);
  EXPECT_FALSE(controller_->IsPromoActive(kTutorialIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, SnoozeServiceBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(0);
  // Simulate a snooze by writing data directly.
  FeaturePromoData data;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_show_time = base::Time::Now();
  data.last_snooze_time = base::Time::Now();
  storage_service()->SavePromoData(kTutorialIPHFeature, data);

  auto result = GetPromoResult(kTutorialIPHFeature);
  EXPECT_FALSE(result);
  CheckNotShownMetrics(kTutorialIPHFeature, result, /*not_shown_count=*/1);
  EXPECT_FALSE(controller_->IsPromoActive(kTutorialIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
  storage_service()->Reset(kTutorialIPHFeature);
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsWhenRequested) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature, close_callback.Get()));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  EXPECT_CALL_IN_SCOPE(
      close_callback, Run(),
      EXPECT_TRUE(controller_->EndPromo(
          kTestIPHFeature,
          user_education::EndFeaturePromoReason::kAbortPromo)));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfPromoNotShowing) {
  EXPECT_FALSE(controller_->EndPromo(
      kTestIPHFeature, user_education::EndFeaturePromoReason::kAbortPromo));
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfDifferentPromoShowing) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));

  EXPECT_FALSE(controller_->EndPromo(
      kTutorialIPHFeature, user_education::EndFeaturePromoReason::kAbortPromo));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsOnBubbleClosure) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature, close_callback.Get()));

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

TEST_F(BrowserFeaturePromoControllerTest,
       ContinuedPromoDefersBackendDismissed) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature, close_callback.Get()));

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

TEST_F(BrowserFeaturePromoControllerTest, ContinuedPromoDismissesOnForceEnd) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  controller_->EndPromo(kTestIPHFeature,
                        user_education::EndFeaturePromoReason::kAbortPromo);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature,
                                          FeaturePromoStatus::kContinued));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  promo_handle.Release();
}

TEST_F(BrowserFeaturePromoControllerTest, PromoHandleDismissesPromoOnRelease) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));

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

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoOnOverwrite) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle = FeaturePromoHandle();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoExactlyOnce) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));

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

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoAfterMoveConstruction) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoHandle promo_handle2(std::move(promo_handle));
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
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoHandle promo_handle2;
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
      .WillOnce(Return(true));

  EXPECT_FALSE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));

  ASSERT_TRUE(GetPromoResult(kTestIPHFeature));
  EXPECT_TRUE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));

  controller_->EndPromo(kTestIPHFeature,
                        user_education::EndFeaturePromoReason::kAbortPromo);
  EXPECT_FALSE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));
}

TEST_F(BrowserFeaturePromoControllerTest, FailsIfBubbleIsShowing) {
  HelpBubbleParams bubble_params;
  bubble_params.body_text = l10n_util::GetStringUTF16(IDS_CHROME_TIP);
  auto bubble = bubble_factory()->CreateHelpBubble(GetAnchorElement(),
                                                   std::move(bubble_params));
  EXPECT_TRUE(bubble);

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  const auto result = GetPromoResult(kTestIPHFeature);
  EXPECT_FALSE(result);
  CheckNotShownMetrics(kTestIPHFeature, result, /*not_shown_count=*/1);
}

// Test that a feature promo can chain into a tutorial.
TEST_F(BrowserFeaturePromoControllerTest, StartsTutorial) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(GetPromoResult(kTutorialIPHFeature));

  // Simulate clicking the "Show Tutorial" button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  // We should be running the tutorial now.
  auto& tutorial_service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
          ->tutorial_service();
  EXPECT_TRUE(tutorial_service.IsRunningTutorial());
  tutorial_service.CancelTutorialIfRunning();
}

// Test that a feature promo can perform a custom action.
TEST_F(BrowserFeaturePromoControllerTest, PerformsCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(GetPromoResult(kCustomActionIPHFeature));

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
TEST_F(BrowserFeaturePromoControllerTest, PerformsCustomActionAsDefault) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kDefaultCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(GetPromoResult(kDefaultCustomActionIPHFeature));

  // Simulate clicking the custom action button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  auto* const button = bubble->GetNonDefaultButtonForTesting(0);
  ASSERT_TRUE(button);

  const std::u16string& text = button->GetText();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOT_NOW), text);

  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  EXPECT_EQ(1, custom_callback_count_);
}

// Test that a feature promo does not perform a custom action when the default
// "Got it" button is clicked.
TEST_F(BrowserFeaturePromoControllerTest, DoesNotPerformCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(GetPromoResult(kCustomActionIPHFeature));

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
TEST_F(BrowserFeaturePromoControllerTest, DoesNotPerformDefaultCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kDefaultCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(GetPromoResult(kDefaultCustomActionIPHFeature));

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
TEST_F(BrowserFeaturePromoControllerTest, CustomActionHidesAnchorView) {
  FeaturePromoHandle promo_handle;
  registry()->RegisterFeature(FeaturePromoSpecification::CreateForCustomAction(
      kCustomActionIPHFeature2, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP,
      IDS_CHROME_TIP,
      base::BindLambdaForTesting(
          [&](ui::ElementContext context, FeaturePromoHandle handle) {
            views::ElementTrackerViews::GetInstance()
                ->GetUniqueView(kToolbarAppMenuButtonElementId, context)
                ->SetVisible(false);
            promo_handle = std::move(handle);
          })));

  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature2)))
      .WillOnce(Return(true));
  ASSERT_TRUE(GetPromoResult(kCustomActionIPHFeature2));

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

TEST_F(BrowserFeaturePromoControllerTest, GetAnchorContext) {
  EXPECT_EQ(browser_view()->GetElementContext(),
            controller_->GetAnchorContext());
}

TEST_F(BrowserFeaturePromoControllerTest, GetAcceleratorProvider) {
  EXPECT_EQ(browser_view(), controller_->GetAcceleratorProvider());
}

TEST_F(BrowserFeaturePromoControllerTest, GetFocusHelpBubbleScreenReaderHint) {
  EXPECT_TRUE(
      controller_
          ->GetFocusHelpBubbleScreenReaderHint(
              FeaturePromoSpecification::PromoType::kToast, GetAnchorElement())
          .empty());
  EXPECT_FALSE(
      controller_
          ->GetFocusHelpBubbleScreenReaderHint(
              FeaturePromoSpecification::PromoType::kSnooze, GetAnchorElement())
          .empty());
}

namespace {
const int kStringWithNoSubstitution = IDS_OK;
const int kStringWithSingleSubstitution =
    IDS_APP_TABLE_COLUMN_SORTED_ASC_ACCNAME;
const int kStringWithMultipleSubstitutions =
    IDS_CONCAT_THREE_STRINGS_WITH_COMMA;
const int kStringWithPluralSubstitution = IDS_TIME_HOURS;
const std::u16string kSubstitution1{u"First"};
const std::u16string kSubstitution2{u"Second"};
const std::u16string kSubstitution3{u"Third"};
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kPromoShownEvent);
}  // namespace

class BrowserFeaturePromoControllerViewsTest
    : public views::test::InteractiveViewsTestT<
          BrowserFeaturePromoControllerTest> {
 public:
  BrowserFeaturePromoControllerViewsTest() = default;
  ~BrowserFeaturePromoControllerViewsTest() override = default;

  void SetUp() override {
    InteractiveViewsTestT<BrowserFeaturePromoControllerTest>::SetUp();
    SetContextWidget(browser_view()->GetWidget());
  }

  auto RegisterPromo(int body_string, int title_string = 0) {
    return Do([this, body_string, title_string]() {
      auto spec = FeaturePromoSpecification::CreateForLegacyPromo(
          &kStringTestIPHFeature, kToolbarAppMenuButtonElementId, body_string);
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

  auto MaybeShowPromo(
      user_education::FeaturePromoParams params,
      FeaturePromoResult expected = FeaturePromoResult::Success()) {
    std::ostringstream desc;
    desc << "MaybeShowPromo(" << params.feature->name << ", " << expected
         << ") - %s";
    auto result =
        base::MakeRefCounted<base::RefCountedData<FeaturePromoResult>>();
    auto steps = Steps(
        WithElement(
            kBrowserViewElementId,
            [this, p = std::move(params), expected,
             result](ui::TrackedElement* el) mutable {
              if (expected) {
                EXPECT_CALL(*mock_tracker_,
                            ShouldTriggerHelpUI(Ref(*p.feature)))
                    .WillOnce(Return(true));
              } else if (expected.failure() ==
                         FeaturePromoResult::kBlockedByConfig) {
                EXPECT_CALL(*mock_tracker_,
                            ShouldTriggerHelpUI(Ref(*p.feature)))
                    .WillOnce(Return(false));
              }

              ui::SafeElementReference el_ref(el);

              p.show_promo_result_callback = base::BindLambdaForTesting(
                  [result,
                   actual_callback = std::move(p.show_promo_result_callback),
                   el_ref = std::move(el_ref)](
                      FeaturePromoResult actual_result) mutable {
                    result->data = actual_result;
                    if (actual_callback) {
                      std::move(actual_callback).Run(actual_result);
                    }
                    auto* const el = el_ref.get();
                    CHECK(el);
                    ui::ElementTracker::GetFrameworkDelegate()
                        ->NotifyCustomEvent(el, kPromoShownEvent);
                  });

              controller_->MaybeShowPromo(std::move(p));
            }),
        WaitForEvent(kBrowserViewElementId, kPromoShownEvent),
        CheckResult([result]() { return result->data; }, expected));
    AddDescription(steps, desc.str());
    return steps;
  }

  auto ClosePromo() {
    return Steps(
        PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
            true));
  }

  auto AbortPromo() {
    return Steps(
        WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                 [](views::View* bubble) { bubble->GetWidget()->Close(); }),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }
};

TEST_F(BrowserFeaturePromoControllerViewsTest,
       BodyTextSubstitution_SingleString) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = kSubstitution1;

  RunTestSequence(
      RegisterPromo(kStringWithSingleSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       BodyTextSubstitution_MultipleStrings) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params =
      user_education::FeaturePromoSpecification::StringSubstitutions{
          kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(
      RegisterPromo(kStringWithMultipleSubstitutions),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithMultipleSubstitutions, kSubstitution1,
                            kSubstitution2, kSubstitution3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, BodyTextSubstitution_Singular) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = 1;

  RunTestSequence(
      RegisterPromo(kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, BodyTextSubstitution_Plural) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = 3;

  RunTestSequence(
      RegisterPromo(kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       TitleTextSubstitution_SingleString) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = kSubstitution1;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithSingleSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       TitleTextSubstitution_MultipleStrings) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params =
      user_education::FeaturePromoSpecification::StringSubstitutions{
          kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithMultipleSubstitutions),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithMultipleSubstitutions, kSubstitution1,
                            kSubstitution2, kSubstitution3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, TitleTextSubstitution_Singular) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = 1;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kTitleTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, TitleTextSubstitution_Plural) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = 3;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kTitleTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       ScreenreaderTextSubstitution_Accelerator) {
  static const ui::Accelerator kAccelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE);
  user_education::FeaturePromoParams params(kStringTestIPHFeature);

  RunTestSequence(RegisterAccessiblePromo(
      kStringWithSingleSubstitution,
      FeaturePromoSpecification::AcceleratorInfo(kAccelerator))),
      MaybeShowPromo(std::move(params)),
      CheckAccessibleText(l10n_util::GetStringFUTF16(
          kStringWithSingleSubstitution, kAccelerator.GetShortcutText()));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       ScreenreaderTextSubstitution_SingleString) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params = kSubstitution1;

  RunTestSequence(RegisterAccessiblePromo(kStringWithSingleSubstitution),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetStringFUTF16(
                      kStringWithSingleSubstitution, kSubstitution1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       ScreenreaderTextSubstitution_MultipleStrings) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params =
      user_education::FeaturePromoSpecification::StringSubstitutions{
          kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(RegisterAccessiblePromo(kStringWithMultipleSubstitutions),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetStringFUTF16(
                      kStringWithMultipleSubstitutions, kSubstitution1,
                      kSubstitution2, kSubstitution3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       ScreenreaderTextSubstitution_Singular) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params = 1;

  RunTestSequence(RegisterAccessiblePromo(kStringWithPluralSubstitution),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetPluralStringFUTF16(
                      kStringWithPluralSubstitution, 1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       ScreenreaderTextSubstitution_Plural) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.screen_reader_params = 3;

  RunTestSequence(RegisterAccessiblePromo(kStringWithPluralSubstitution),
                  MaybeShowPromo(std::move(params)),
                  CheckAccessibleText(l10n_util::GetPluralStringFUTF16(
                      kStringWithPluralSubstitution, 3)));
}

namespace {

BASE_FEATURE(kRotatingPromoIPHFeature,
             "TEST_RotatingPromoIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

}

class BrowserFeaturePromoControllerRotatingPromoTest
    : public BrowserFeaturePromoControllerViewsTest {
 public:
  BrowserFeaturePromoControllerRotatingPromoTest() = default;
  ~BrowserFeaturePromoControllerRotatingPromoTest() override = default;

  template <typename... Args>
  void RegisterRotatingPromo(Args&&... args) {
    registry()->clear_features_for_testing();
    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateRotatingPromoForTesting(
            kRotatingPromoIPHFeature, FeaturePromoSpecification::RotatingPromos(
                                          std::forward<Args>(args)...)));
  }

  auto VerifyPromoData(
      int show_count,
      int snooze_count,
      std::optional<FeaturePromoClosedReason> last_closed_reason) {
    auto result =
        Steps(WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),

              CheckResult([this]() { return GetData().show_count; }, show_count,
                          "Check show count."),
              CheckResult([this]() { return GetData().snooze_count; },
                          snooze_count, "Check snooze count."));
    if (last_closed_reason) {
      result.emplace_back(
          CheckResult([this]() { return GetData().last_dismissed_by; },
                      *last_closed_reason, "Check close reason."));
    }
    return result;
  }

  auto VerifyHasHelpBubble(ui::ElementIdentifier id) {
    return CheckView(id, [](views::View* view) {
      return view->GetProperty(user_education::kHasInProductHelpPromoKey);
    });
  }

 private:
  FeaturePromoData GetData() {
    auto result = storage_service()->ReadPromoData(kRotatingPromoIPHFeature);
    CHECK(result.has_value());
    return *result;
  }
};

TEST_F(BrowserFeaturePromoControllerRotatingPromoTest, OnePromo) {
  RegisterRotatingPromo(FeaturePromoSpecification::CreateForSnoozePromo(
      kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
      IDS_CHROME_TIP));

  // Show the rotating promo twice, closing it different ways. The same text
  // should re-show each time.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo(),
                  VerifyPromoData(1, 0, FeaturePromoClosedReason::kCancel),
                  MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
                  VerifyPromoData(2, 0, FeaturePromoClosedReason::kDismiss));
}

TEST_F(BrowserFeaturePromoControllerRotatingPromoTest, ToastHasDismissButton) {
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_CANCEL,
          FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForSnoozePromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK));

  // Show the rotating promo three times, verifying that it wraps around to the,
  // first promo after the second.
  RunTestSequence(
      // Show the promo and press the default button to close it.
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Ensure the next promo shows.
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_OK)));
}

TEST_F(BrowserFeaturePromoControllerRotatingPromoTest, TwoPromosRotating) {
  int call_count = 0;
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForSnoozePromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP),
      FeaturePromoSpecification::CreateForCustomAction(
          kRotatingPromoIPHFeature, kTopContainerElementId, IDS_OK,
          IDS_CHROME_TIP,
          base::BindLambdaForTesting(
              [&call_count](ui::ElementContext, FeaturePromoHandle) {
                ++call_count;
              })));

  // Show the rotating promo three times, verifying that it wraps around to the,
  // first promo after the second.
  RunTestSequence(
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId), ClosePromo(),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_OK)),
      VerifyHasHelpBubble(kTopContainerElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      CheckResult([&call_count]() { return call_count; }, 1),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      CheckResult([&call_count]() { return call_count; }, 1));
}

TEST_F(BrowserFeaturePromoControllerRotatingPromoTest, SnoozeButtonRepeats) {
  int call_count = 0;
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForSnoozePromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP),
      FeaturePromoSpecification::CreateForCustomAction(
          kRotatingPromoIPHFeature, kTopContainerElementId, IDS_OK,
          IDS_CHROME_TIP,
          base::BindLambdaForTesting(
              [&call_count](ui::ElementContext, FeaturePromoHandle) {
                ++call_count;
              })));

  // Show the rotating promo three times, snoozing the first time. Verify that
  // snoozing re-shows the same promo.
  RunTestSequence(
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      VerifyPromoData(1, 1, std::nullopt),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId), ClosePromo(),
      VerifyPromoData(2, 1, FeaturePromoClosedReason::kCancel),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_OK)),
      VerifyHasHelpBubble(kTopContainerElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      CheckResult([&call_count]() { return call_count; }, 1));
}

TEST_F(BrowserFeaturePromoControllerRotatingPromoTest, RotatesPastGaps) {
  RegisterRotatingPromo(
      std::nullopt,
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      std::nullopt,
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()),
      std::nullopt, std::nullopt);

  // Show the rotating promo three times, verifying that it skips gaps.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo(), MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_OK)),
                  ClosePromo(), MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo());
}

TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
       ContinuesWithNewRotatingPromo) {
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show the two existing promos, putting the promo at the end of the list.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo(), MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_OK)),
                  ClosePromo());

  // Re-register with an additional promo.
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show one more promo; it should be the new one.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CANCEL)),
                  ClosePromo());
}

TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
       ContinuesAfterPromoRemoved) {
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show the first promo, putting the promo at the second index.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo());

  // Re-register with the second promo removed.
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      std::nullopt,
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show one more promo; it should be the third.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CANCEL)),
                  ClosePromo());
}

namespace {
// Somewhere around 2020.
const base::Time kSessionStartTime =
    base::Time::FromDeltaSinceWindowsEpoch(420 * base::Days(365));

BASE_FEATURE(kLegalNoticeFeature,
             "LegalNoticeFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLegalNoticeFeature2,
             "LegalNoticeFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActionableAlertIPHFeature,
             "kActionableAlertIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActionableAlertIPHFeature2,
             "kActionableAlertIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);

class StartupCallbackObserver : public ui::test::StateObserver<bool> {
 public:
  StartupCallbackObserver(
      base::MockCallback<FeaturePromoController::ShowPromoResultCallback>*
          callback,
      FeaturePromoResult expected) {
    EXPECT_CALL(*callback, Run)
        .WillOnce([this, expected](FeaturePromoResult result) {
          ASSERT_EQ(expected, result);
          OnStateObserverStateChanged(true);
        });
  }
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(StartupCallbackObserver,
                                    kStartupCallbackState);

class RequiredNotice {
 public:
  explicit RequiredNotice(Browser* browser)
      : controller_(UserEducationServiceFactory::GetForBrowserContext(
                        browser->profile())
                        ->product_messaging_controller()) {}
  RequiredNotice(const RequiredNotice&) = delete;
  void operator=(const RequiredNotice&) = delete;
  ~RequiredNotice() = default;

  void Request(user_education::RequiredNoticeId id) {
    CHECK(!id_);
    CHECK(!handle_);
    id_ = id;
    controller_->QueueRequiredNotice(
        id_, base::BindOnce(&RequiredNotice::OnPriority,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void Release() {
    CHECK(handle_);
    CHECK(!id_);
    handle_.Release();
  }

  bool has_priority() const { return !!handle_; }

  auto AddOnPriorityCallback(base::OnceClosure callback) {
    return callbacks_.Add(std::move(callback));
  }

 private:
  void OnPriority(user_education::RequiredNoticePriorityHandle handle) {
    handle_ = std::move(handle);
    id_ = user_education::RequiredNoticeId();
    callbacks_.Notify();
  }

  user_education::RequiredNoticeId id_;
  user_education::RequiredNoticePriorityHandle handle_;
  base::OnceCallbackList<void()> callbacks_;
  const raw_ref<user_education::ProductMessagingController> controller_;
  base::WeakPtrFactory<RequiredNotice> weak_ptr_factory_{this};
};

class NoticeCallbackObserver : public ui::test::StateObserver<bool> {
 public:
  explicit NoticeCallbackObserver(RequiredNotice* notice)
      : sub_(notice->AddOnPriorityCallback(
            base::BindOnce(&NoticeCallbackObserver::OnNotice,
                           base::Unretained(this)))) {}

 private:
  void OnNotice() { OnStateObserverStateChanged(true); }

  base::CallbackListSubscription sub_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(NoticeCallbackObserver,
                                    kNoticeCallbackState);

DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kRequiredNoticeId);

}  // namespace

class BrowserFeaturePromoControllerPriorityTest
    : public BrowserFeaturePromoControllerViewsTest {
 public:
  BrowserFeaturePromoControllerPriorityTest() { VerifyConstants(); }
  ~BrowserFeaturePromoControllerPriorityTest() override = default;

  void TearDown() override {
    test_util_.reset();
    BrowserFeaturePromoControllerViewsTest::TearDown();
  }

 protected:
  bool UseV2() const override { return true; }

  void RegisterIPH() override {
    BrowserFeaturePromoControllerViewsTest::RegisterIPH();

    FeaturePromoSpecification spec =
        DefaultPromoSpecification(kLegalNoticeFeature);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForTutorialPromo(
        kLegalNoticeFeature2, kToolbarAppMenuButtonElementId, IDS_OK,
        kTestTutorialIdentifier);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForCustomAction(
        kActionableAlertIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
        IDS_OK, base::DoNothing());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kActionableAlert);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForCustomAction(
        kActionableAlertIPHFeature2, kToolbarAppMenuButtonElementId, IDS_CANCEL,
        IDS_OK, base::DoNothing());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kActionableAlert);
    registry()->RegisterFeature(std::move(spec));
  }

  auto MaybeShowStartupPromo(user_education::FeaturePromoParams params,
                             bool expected = true) {
    std::ostringstream desc;
    desc << "MaybeShowStartupPromo(" << params.feature->name << ", "
         << (expected ? "true" : "false") << ")";
    return CheckResult(
        [this, p = std::move(params), expected]() mutable {
          if (expected) {
            // This is insurance, a parameter could be added to specify whether
            // the feature is expected to check the tracker or not.
            EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(*p.feature)))
                .WillRepeatedly(Return(true));
          }
          return controller_->MaybeShowStartupPromo(std::move(p));
        },
        expected, desc.str());
  }

  auto ExpectShowingPromo(const base::Feature* feature) {
    return CheckResult(
        [this]() { return controller()->GetCurrentPromoFeature(); }, feature,
        base::StringPrintf("ExpectShowingPromo %s",
                           feature ? feature->name : "[none]"));
  }

  auto ResetSessionData(base::TimeDelta since_session_start,
                        base::TimeDelta idle_time = base::Seconds(1)) {
    return std::move(
        WithView(kBrowserViewElementId, [this, since_session_start,
                                         idle_time](BrowserView* browser_view) {
          FeaturePromoSessionData session_data;
          session_data.start_time = kSessionStartTime;
          session_data.most_recent_active_time =
              kSessionStartTime + since_session_start;
          now_ = session_data.most_recent_active_time + idle_time;
          FeaturePromoPolicyData policy_data;
          test_util_ = std::make_unique<
              user_education::test::FeaturePromoSessionTestUtil>(
              UserEducationServiceFactory::GetForBrowserContext(
                  browser_view->GetProfile())
                  ->feature_promo_session_manager(),
              session_data, policy_data, session_data.most_recent_active_time,
              now_);
        }).SetDescription("ResetSessionData"));
  }

  auto AdvanceTime(std::optional<base::TimeDelta> until_new_last_active,
                   base::TimeDelta until_new_now = base::Milliseconds(500),
                   bool send_update = true) {
    return Do([this, until_new_last_active, until_new_now, send_update]() {
      const auto new_active_time =
          until_new_last_active
              ? std::make_optional(now_ + *until_new_last_active)
              : std::nullopt;
      now_ = new_active_time.value_or(now_) + until_new_now;
      test_util_->SetNow(now_);
      if (new_active_time) {
        test_util_->UpdateLastActiveTime(*new_active_time, send_update);
      }
    });
  }

  auto CheckPromoStatus(const base::Feature& iph_feature,
                        FeaturePromoStatus status) {
    std::ostringstream oss;
    oss << "CheckPromoStatus(" << iph_feature.name << ", " << status << ")";
    return CheckResult(
        [this, &iph_feature]() {
          return controller()->GetPromoStatus(iph_feature);
        },
        status, oss.str());
  }

  const base::TimeDelta kLessThanGracePeriod =
      user_education::features::GetSessionStartGracePeriod() / 4;
  const base::TimeDelta kMoreThanGracePeriod =
      user_education::features::GetSessionStartGracePeriod() + base::Minutes(5);
  const base::TimeDelta kLessThanCooldown =
      user_education::features::GetLowPriorityCooldown() / 4;
  const base::TimeDelta kMoreThanCooldown =
      user_education::features::GetLowPriorityCooldown() + base::Hours(1);
  const base::TimeDelta kMoreThanSnooze =
      user_education::features::GetSnoozeDuration() + base::Hours(1);
  const base::TimeDelta kLessThanAbortCooldown =
      user_education::features::GetAbortCooldown() / 2;
  const base::TimeDelta kMoreThanAbortCooldown =
      user_education::features::GetAbortCooldown() + base::Minutes(5);
  const base::TimeDelta kLessThanNewSession =
      user_education::features::GetIdleTimeBetweenSessions() / 4;
  const base::TimeDelta kMoreThanNewSession =
      user_education::features::GetIdleTimeBetweenSessions() + base::Hours(1);

 private:
  // Ensures some basic orderings of values to avoid triggering unexpected
  // behavior.
  void VerifyConstants() {
    CHECK_GT(kLessThanCooldown, kMoreThanNewSession);
    CHECK_LT(kMoreThanGracePeriod + kLessThanCooldown,
             user_education::features::GetLowPriorityCooldown());
    CHECK_LT(kMoreThanAbortCooldown + kMoreThanGracePeriod,
             user_education::features::GetSnoozeDuration());
  }

  std::unique_ptr<user_education::test::FeaturePromoSessionTestUtil> test_util_;
  base::Time now_;
};

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       MultipleStartupPromosHighPriority) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowStartupPromo(kLegalNoticeFeature),
                  MaybeShowStartupPromo(kLegalNoticeFeature2),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kLegalNoticeFeature),
                  // This is required so we don't try to close on the same call
                  // stack as the bubble was shown on.
                  ClosePromo(),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kLegalNoticeFeature2), ClosePromo());
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       MultipleStartupPromosHighPriorityToastThenLowPriorityAllowed) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  user_education::FeaturePromoParams second_params(kSnoozeIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RunTestSequence(  // Since the second promo cannot show during grace period,
                    // assume this is a browser restart during a session.
      ResetSessionData(kMoreThanGracePeriod),
      ObserveState(kStartupCallbackState, &second_promo_callback,
                   FeaturePromoResult::Success()),
      MaybeShowStartupPromo(kLegalNoticeFeature),
      MaybeShowStartupPromo(std::move(second_params)),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kLegalNoticeFeature),
      // This is required so we don't try to close on the same call
      // stack as the bubble was shown on.
      ClosePromo(), WaitForState(kStartupCallbackState, true),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kSnoozeIPHFeature), ClosePromo());
}

TEST_F(
    BrowserFeaturePromoControllerPriorityTest,
    MultipleStartupPromosHighPriorityLowPriorityToastAllowedAfterHeavyweight) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  user_education::FeaturePromoParams second_params(kTestIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  ObserveState(kStartupCallbackState, &second_promo_callback,
                               FeaturePromoResult::Success()),
                  MaybeShowStartupPromo(kLegalNoticeFeature2),
                  MaybeShowStartupPromo(std::move(second_params)),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kLegalNoticeFeature2),
                  // This is required so we don't try to close on the same call
                  // stack as the bubble was shown on.
                  ClosePromo(), WaitForState(kStartupCallbackState, true),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kTestIPHFeature), ClosePromo());
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       MultipleStartupPromosHighPriorityLowPriorityBlockedAfterHeavyweight) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  user_education::FeaturePromoParams second_params(kSnoozeIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RunTestSequence(  // Since the second promo cannot show during grace period,
                    // assume this is a browser restart during a session.
      ResetSessionData(kMoreThanGracePeriod),
      ObserveState(kStartupCallbackState, &second_promo_callback,
                   FeaturePromoResult::kBlockedByCooldown),
      MaybeShowStartupPromo(kLegalNoticeFeature2),
      MaybeShowStartupPromo(std::move(second_params)),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kLegalNoticeFeature2),
      // This is required so we don't try to close on the same call
      // stack as the bubble was shown on.
      ClosePromo(), WaitForState(kStartupCallbackState, true));
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       RequiredNoticeDelaysLegalNotice) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         promo_callback);
  user_education::FeaturePromoParams params(kLegalNoticeFeature);
  params.show_promo_result_callback = promo_callback.Get();
  RequiredNotice notice(browser());
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      ObserveState(kStartupCallbackState, &promo_callback,
                   FeaturePromoResult::Success()),
      ObserveState(kNoticeCallbackState, &notice),
      // Request notice first before startup promo.
      Do([&notice]() { notice.Request(kRequiredNoticeId); }),
      MaybeShowStartupPromo(std::move(params)),
      // Wait for notice to pop and ensure that the startup promo wasn't shown.
      WaitForState(kNoticeCallbackState, true),
      WaitForState(kStartupCallbackState, false),
      // Release the notice and verify the promo shows.
      Do([&notice]() { notice.Release(); }),
      WaitForState(kStartupCallbackState, true),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ClosePromo());
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       LegalNoticeDelaysRequiredNotice) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         promo_callback);
  RequiredNotice notice(browser());
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  ObserveState(kNoticeCallbackState, &notice),
                  MaybeShowStartupPromo(kLegalNoticeFeature),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  Do([&notice]() { notice.Request(kRequiredNoticeId); }),
                  WaitForState(kNoticeCallbackState, false), ClosePromo(),
                  WaitForState(kNoticeCallbackState, true),
                  Do([&notice]() { notice.Release(); }));
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       MultipleStartupPromosHighThenNoticeThenLow) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  user_education::FeaturePromoParams second_params(kTestIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RequiredNotice notice(browser());
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Observe the notice and the second callback.
      ObserveState(kNoticeCallbackState, &notice),
      ObserveState(kStartupCallbackState, &second_promo_callback,
                   FeaturePromoResult::Success()),
      // Queue both promos and wait for the first to show.
      MaybeShowStartupPromo(kLegalNoticeFeature2),
      MaybeShowStartupPromo(std::move(second_params)),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kLegalNoticeFeature2),
      // Request the notice and verify it doesn't pop.
      Do([&notice]() { notice.Request(kRequiredNoticeId); }),
      WaitForState(kNoticeCallbackState, false),
      // Close the promo and verify the notice (and not the other promo) pops.
      ClosePromo(), WaitForState(kNoticeCallbackState, true),
      WaitForState(kStartupCallbackState, false),
      // Release the notice and verify the final promo is shown.
      Do([&notice]() { notice.Release(); }),
      WaitForState(kStartupCallbackState, true),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kTestIPHFeature), ClosePromo());
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       RegularPromoBlockedWhenPromoIsQueued) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowStartupPromo(kLegalNoticeFeature),
      MaybeShowPromo(kTutorialIPHFeature, FeaturePromoResult::kBlockedByPromo));
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       LegalNoticeNotBlockedWhenPromoIsQueued) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowStartupPromo(kSnoozeIPHFeature),
                  MaybeShowPromo(kLegalNoticeFeature));
}

TEST_F(BrowserFeaturePromoControllerPriorityTest,
       SecondPromoNotCanceledWhenFirstQueuedPromoIsOverridden) {
  SetTrackerInitBehavior(true, TrackerCallbackBehavior::kPost);
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowStartupPromo(kSnoozeIPHFeature),
      MaybeShowStartupPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),

      CheckPromoStatus(kSnoozeIPHFeature, FeaturePromoStatus::kBubbleShowing),
      InParallel(
          MaybeShowPromo(kLegalNoticeFeature),
          WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting, true)),

      CheckPromoStatus(kLegalNoticeFeature, FeaturePromoStatus::kBubbleShowing),
      CheckPromoStatus(kSnoozeIPHFeature, FeaturePromoStatus::kNotRunning),
      CheckPromoStatus(kTutorialIPHFeature,
                       FeaturePromoStatus::kQueuedForStartup));
}

namespace {
BASE_FEATURE(kKeyedPromoFeature,
             "KeyedPromoFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kKeyedPromoFeature2,
             "KeyedPromoFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kAppName1[] = "app1";
constexpr char kAppName2[] = "app2";
}  // namespace

class BrowserFeaturePromoControllerReshowTest
    : public BrowserFeaturePromoControllerPriorityTest {
 public:
  BrowserFeaturePromoControllerReshowTest() = default;
  ~BrowserFeaturePromoControllerReshowTest() override = default;

  void RegisterIPH() override {
    BrowserFeaturePromoControllerViewsTest::RegisterIPH();

    FeaturePromoSpecification spec =
        DefaultPromoSpecification(kLegalNoticeFeature);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    spec.SetReshowPolicy(base::Days(20), std::nullopt);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForTutorialPromo(
        kLegalNoticeFeature2, kToolbarAppMenuButtonElementId, IDS_OK,
        kTestTutorialIdentifier);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    spec.SetReshowPolicy(base::Days(100), 2);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForCustomAction(
        kKeyedPromoFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL, IDS_OK,
        base::DoNothing());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kKeyedNotice);
    spec.SetReshowPolicy(base::Days(100), std::nullopt);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForToastPromo(
        kKeyedPromoFeature2, kToolbarAppMenuButtonElementId, IDS_CANCEL, IDS_OK,
        FeaturePromoSpecification::AcceleratorInfo());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kKeyedNotice);
    spec.SetReshowPolicy(base::Days(20), 2);
    registry()->RegisterFeature(std::move(spec));
  }
};

TEST_F(BrowserFeaturePromoControllerReshowTest, ReshowLegalNoticeWithNoLimit) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Promo can show initially.
                  MaybeShowPromo(kLegalNoticeFeature), ClosePromo(),
                  // Promo cannot reshow immediately.
                  MaybeShowPromo(kLegalNoticeFeature,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo cannot reshow after a short period.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo can reshow after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(20)),
                  MaybeShowPromo(kLegalNoticeFeature), ClosePromo(),
                  // Promo cannot reshow again after a short time.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo can reshow again after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(20)),
                  MaybeShowPromo(kLegalNoticeFeature), ClosePromo());
}

TEST_F(BrowserFeaturePromoControllerReshowTest, ReshowLegalNoticeWithLimit) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Promo can show initially.
                  MaybeShowPromo(kLegalNoticeFeature2), ClosePromo(),
                  // Promo cannot reshow immediately.
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo cannot reshow after a short period.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo can reshow after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(100)),
                  MaybeShowPromo(kLegalNoticeFeature2), ClosePromo(),
                  // Promo cannot reshow again because it has reached the limit.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kPermanentlyDismissed),
                  AdvanceTime(std::nullopt, base::Days(100)),
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kPermanentlyDismissed));
}

TEST_F(BrowserFeaturePromoControllerReshowTest, ReshowKeyedPromoNoLimit) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Promo can show initially.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1}), ClosePromo(),
                  // Promo cannot reshow immediately.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1},
                                 FeaturePromoResult::kBlockedByReshowDelay),

                  // Promo cannot reshow after a short period.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1},
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // But for other app it can.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName2}), ClosePromo(),

                  // Promo can reshow after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(99)),
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1}), ClosePromo(),

                  // But other app cannot, since it has not been long enough.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName2},
                                 FeaturePromoResult::kBlockedByReshowDelay));
}

TEST_F(BrowserFeaturePromoControllerReshowTest, ReshowKeyedPromoWithLimit) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Promo can show initially.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1}), ClosePromo(),
      // Promo cannot reshow immediately.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1},
                     FeaturePromoResult::kBlockedByReshowDelay),

      // Promo cannot reshow after a short period.
      AdvanceTime(std::nullopt, base::Days(5)),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1},
                     FeaturePromoResult::kBlockedByReshowDelay),
      // But for other app it can.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2}), ClosePromo(),

      // Promo can reshow after sufficient time.
      AdvanceTime(std::nullopt, base::Days(19)),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1}), ClosePromo(),

      // But other app cannot, since it has not been long enough.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2},
                     FeaturePromoResult::kBlockedByReshowDelay),

      // After additional time, the second app can show, but the
      // first has hit the limit.
      AdvanceTime(std::nullopt, base::Days(25)),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1},
                     FeaturePromoResult::kPermanentlyDismissed),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2}), ClosePromo(),

      // Both are now permanently dismissed.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2},
                     FeaturePromoResult::kPermanentlyDismissed));
}

class BrowserFeaturePromoControllerPolicyTest
    : public BrowserFeaturePromoControllerPriorityTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserFeaturePromoControllerPolicyTest() = default;

  ~BrowserFeaturePromoControllerPolicyTest() override = default;

  void TearDown() override {
    help_bubble_.reset();
    BrowserFeaturePromoControllerPriorityTest::TearDown();
  }

  auto SimulateSnoozes(const base::Feature& feature, int delta_from_max) {
    return Do([this, &feature, delta_from_max] {
      auto data = storage_service()->ReadPromoData(feature);
      if (!data) {
        data = FeaturePromoData();
      }
      data->show_count = data->snooze_count =
          user_education::features::GetMaxSnoozeCount() + delta_from_max;
      storage_service()->SavePromoData(feature, *data);
    });
  }

  bool is_help_bubble_open() const {
    return help_bubble_ && help_bubble_->is_open();
  }

  auto ShowHelpBubble() {
    return Check(
        [this]() {
          HelpBubbleParams bubble_params;
          bubble_params.body_text = l10n_util::GetStringUTF16(IDS_CHROME_TIP);
          help_bubble_ = bubble_factory()->CreateHelpBubble(
              GetAnchorElement(), std::move(bubble_params));
          return is_help_bubble_open();
        },
        "ShowHelpBubble()");
  }

 protected:
  bool UseV2() const override { return GetParam(); }

 private:
  std::unique_ptr<user_education::HelpBubble> help_bubble_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserFeaturePromoControllerPolicyTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& param) {
                           return param.param ? "V2" : "Legacy";
                         });

TEST_P(BrowserFeaturePromoControllerPolicyTest, TwoLowPriorityPromos) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature),
                  ExpectShowingPromo(&kTestIPHFeature),
                  MaybeShowPromo(kCustomActionIPHFeature,
                                 FeaturePromoResult::kBlockedByPromo),
                  ExpectShowingPromo(&kTestIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       ActionableAlertOverridesLowPriority) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature),
                  MaybeShowPromo(kActionableAlertIPHFeature),
                  ExpectShowingPromo(&kActionableAlertIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, TwoActionableAlerts) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kActionableAlertIPHFeature),
                  ExpectShowingPromo(&kActionableAlertIPHFeature),
                  MaybeShowPromo(kActionableAlertIPHFeature2,
                                 FeaturePromoResult::kBlockedByPromo),
                  ExpectShowingPromo(&kActionableAlertIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       LegalNoticeOverridesLowPriority) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature),
                  MaybeShowPromo(kLegalNoticeFeature),
                  ExpectShowingPromo(&kLegalNoticeFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       LegalNoticeOverridesActionableAlert) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kActionableAlertIPHFeature),
                  MaybeShowPromo(kLegalNoticeFeature),
                  ExpectShowingPromo(&kLegalNoticeFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, TwoLegalNotices) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowPromo(kLegalNoticeFeature),
      ExpectShowingPromo(&kLegalNoticeFeature),
      MaybeShowPromo(kLegalNoticeFeature2, FeaturePromoResult::kBlockedByPromo),
      ExpectShowingPromo(&kLegalNoticeFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodBlocksHeavyweightInV2) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod),
      MaybeShowPromo(kTutorialIPHFeature,
                     UseV2() ? FeaturePromoResult::kBlockedByGracePeriod
                             : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodDoesNotBlockLightweightInV2) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod),
      MaybeShowPromo(kTestIPHFeature, FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodDoesNotBlockHeavyweightLegalNotice) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod),
      MaybeShowPromo(kLegalNoticeFeature2, FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodDoesNotBlockActionableAlert) {
  RunTestSequence(ResetSessionData(kLessThanGracePeriod),
                  MaybeShowPromo(kActionableAlertIPHFeature2,
                                 FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodBlocksHeavyweightInV2AfterNewSession) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod), AdvanceTime(kMoreThanNewSession),
      MaybeShowPromo(kTutorialIPHFeature,
                     UseV2() ? FeaturePromoResult::kBlockedByGracePeriod
                             : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodDoesNotBlocksHeavyweightLongAfterNewSession) {
  RunTestSequence(
      ResetSessionData(base::Seconds(60)), AdvanceTime(kMoreThanNewSession),
      AdvanceTime(kMoreThanGracePeriod),
      MaybeShowPromo(kTutorialIPHFeature, FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, CooldownPreventsPromoInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
      AdvanceTime(kLessThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      MaybeShowPromo(kCustomActionIPHFeature,
                     UseV2() ? FeaturePromoResult::kBlockedByCooldown
                             : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       CooldownDoesNotPreventLightweightPromo) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       LightweightPromoDoesNotTriggerCooldown) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod), MaybeShowPromo(kTestIPHFeature),
      ClosePromo(), AdvanceTime(kLessThanCooldown),
      AdvanceTime(kMoreThanGracePeriod), MaybeShowPromo(kTutorialIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       CooldownDoesNotPreventLegalNotice) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kLegalNoticeFeature2));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       CooldownDoesNotPreventActionableAlert) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kActionableAlertIPHFeature2));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       ExpiredCooldownDoesNotPreventPromo) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kMoreThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kCustomActionIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       AbortedPromoDoesNotTriggerCooldown) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Show an immediately close the promo without user
                  // interaction.
                  MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
                  // Immediately try another promo.
                  MaybeShowPromo(kCustomActionIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       AbortedPromoDoesTriggerIndividualCooldown) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
                  AdvanceTime(kLessThanAbortCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 FeaturePromoResult::kRecentlyAborted));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       AbortedPromoDoesNotTriggerSnooze) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
      AdvanceTime(kMoreThanAbortCooldown), AdvanceTime(kMoreThanGracePeriod),
      // V1 uses full snooze time for aborted promos.
      MaybeShowPromo(kTutorialIPHFeature,
                     UseV2() ? FeaturePromoResult::Success()
                             : FeaturePromoResult::kRecentlyAborted));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, SnoozeButtonDisappearsInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Simulate N-1 snoozes at some distant time in the past.
      SimulateSnoozes(kSnoozeIPHFeature, -1),
      // Show a snoozeable promo, verify the snooze button is
      // present, and press it.
      MaybeShowPromo(kSnoozeIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),

      // Wait until after the snooze period expires. We should now
      // be at N snoozes.
      AdvanceTime(kMoreThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      // Show the promo again and verify that in V2 the snooze
      // button is *not* present.
      MaybeShowPromo(kSnoozeIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      If([this] { return UseV2(); },
         EnsureNotPresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
         EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting)));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       TutorialSnoozeButtonChangesInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Simulate N-1 snoozes at some distant time in the past.
      SimulateSnoozes(kTutorialIPHFeature, -1),
      // Show a snoozeable promo, verify the snooze button is
      // present, and press it.
      MaybeShowPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),

      // Wait until after the snooze period expires. We should now
      // be at N snoozes.
      AdvanceTime(kMoreThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      // Show the promo again and verify that in V2 the snooze
      // button is *not* present.
      MaybeShowPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(
          HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(UseV2() ? IDS_PROMO_DISMISS_BUTTON
                                            : IDS_PROMO_SNOOZE_BUTTON)));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, IdleAtStartupStillShowsPromo) {
  RunTestSequence(
      ResetSessionData(base::TimeDelta()),
      AdvanceTime(std::nullopt, kLessThanNewSession, true),
      AdvanceTime(base::Seconds(15), base::Milliseconds(100), false),
      MaybeShowPromo(kTutorialIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       IdleAtStartupPromoBlockedByNewSession) {
  RunTestSequence(
      ResetSessionData(base::TimeDelta()),
      AdvanceTime(std::nullopt, kMoreThanNewSession, true),
      AdvanceTime(base::Seconds(15), base::Milliseconds(100), false),
      MaybeShowPromo(kTutorialIPHFeature,
                     UseV2() ? FeaturePromoResult::kBlockedByGracePeriod
                             : FeaturePromoResult::Success()));
}
