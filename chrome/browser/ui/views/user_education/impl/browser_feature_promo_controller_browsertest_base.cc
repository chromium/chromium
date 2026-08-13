// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_browsertest_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/test/user_education_session_test_util.h"
#include "components/user_education/views/help_bubble_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace user_education {

namespace test {

BASE_FEATURE(kTestIPHFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSnoozeIPHFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTutorialIPHFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionIPHFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultCustomActionIPHFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLegalNoticeFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLegalNoticeFeature2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActionableAlertIPHFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActionableAlertIPHFeature2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kKeyedPromoFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kKeyedPromoFeature2, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kRotatingPromoIPHFeature, base::FEATURE_ENABLED_BY_DEFAULT);

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kPromoShownEvent);

}  // namespace test

namespace {

using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

// Somewhere around 2020.
const base::Time kSessionStartTime =
    base::Time::FromDeltaSinceWindowsEpoch(420 * base::Days(365));

}  // namespace

BrowserFeaturePromoControllerTestBase::BrowserFeaturePromoControllerTestBase()
    : kLessThanGracePeriod(features::GetSessionStartGracePeriod() / 4),
      kMoreThanGracePeriod(features::GetSessionStartGracePeriod() +
                           base::Minutes(5)),
      kLessThanCooldown(features::GetLowPriorityCooldown() / 4),
      kMoreThanCooldown(features::GetLowPriorityCooldown() + base::Hours(1)),
      kMoreThanSnooze(features::GetSnoozeDuration() + base::Hours(1)),
      kLessThanAbortCooldown(features::GetAbortCooldown() / 2),
      kMoreThanAbortCooldown(features::GetAbortCooldown() + base::Minutes(5)),
      kLessThanNewSession(features::GetIdleTimeBetweenSessions() / 4),
      kMoreThanNewSession(features::GetIdleTimeBetweenSessions() +
                          base::Hours(1)) {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&BrowserFeaturePromoControllerTestBase::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

BrowserFeaturePromoControllerTestBase::
    ~BrowserFeaturePromoControllerTestBase() = default;

void BrowserFeaturePromoControllerTestBase::SetUp() {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  // Disable all registered IPH. These tests use only test features.
  for (const auto& feature : feature_engagement::GetAllFeatures()) {
    disabled_features.emplace_back(*feature);
  }

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  InteractiveBrowserTest::SetUp();
}

void BrowserFeaturePromoControllerTestBase::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
  auto* const service = UserEducationServiceFactory::GetForBrowserContext(
      browser()->GetProfile());
  auto* const interface = BrowserUserEducationInterface::From(browser());
  controller_ = static_cast<FeaturePromoControllerImpl*>(
      service->GetFeaturePromoControllerForTesting());
  user_education_context_ = interface->GetUserEducationContextForTesting();
  lock_ = FeaturePromoControllerImpl::BlockActiveWindowCheckForTesting();

  mock_tracker_ = static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser()->GetProfile()));
  EXPECT_CALL(*mock_tracker_, IsInitialized).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillRepeatedly(
          [](feature_engagement::Tracker::OnInitializedCallback
                 on_initialized) { std::move(on_initialized).Run(true); });

  registry()->clear_features_for_testing();

  // Ensure that the new profile grace period has ended by default.
  auto& storage = service->user_education_storage_service();
  storage.set_profile_creation_time_for_testing(storage.GetCurrentTime() -
                                                base::Days(365));

  // Create a dummy tutorial.
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

  service->tutorial_registry().AddTutorial(test::kTestTutorialIdentifier,
                                           std::move(desc));

  RegisterIPH();
}

void BrowserFeaturePromoControllerTestBase::TearDownOnMainThread() {
  test_util_.reset();
  controller_ = nullptr;
  mock_tracker_ = nullptr;
  lock_.reset();
  InteractiveBrowserTest::TearDownOnMainThread();
}

void BrowserFeaturePromoControllerTestBase::RegisterIPH() {
  registry()->RegisterFeature(DefaultPromoSpecification(test::kTestIPHFeature));

  registry()->RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      test::kSnoozeIPHFeature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP));

  registry()->RegisterFeature(FeaturePromoSpecification::CreateForTutorialPromo(
      test::kTutorialIPHFeature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP,
      test::kTestTutorialIdentifier));

  registry()->RegisterFeature(FeaturePromoSpecification::CreateForCustomAction(
      test::kCustomActionIPHFeature, kToolbarAppMenuButtonElementId,
      IDS_CHROME_TIP, IDS_CHROME_TIP,
      base::BindRepeating(
          &BrowserFeaturePromoControllerTestBase::OnCustomPromoAction,
          base::Unretained(this),
          base::Unretained(&test::kCustomActionIPHFeature))));

  auto default_custom = FeaturePromoSpecification::CreateForCustomAction(
      test::kDefaultCustomActionIPHFeature, kToolbarAppMenuButtonElementId,
      IDS_CHROME_TIP, IDS_CHROME_TIP,
      base::BindRepeating(
          &BrowserFeaturePromoControllerTestBase::OnCustomPromoAction,
          base::Unretained(this),
          base::Unretained(&test::kDefaultCustomActionIPHFeature)));
  default_custom.SetCustomActionIsDefault(true);
  default_custom.SetCustomActionDismissText(IDS_NOT_NOW);
  registry()->RegisterFeature(std::move(default_custom));
}

BrowserView* BrowserFeaturePromoControllerTestBase::browser_view() {
  return BrowserView::GetBrowserViewForBrowser(browser());
}

InteractiveBrowserTest::StepBuilder
BrowserFeaturePromoControllerTestBase::ResetSessionData(
    base::TimeDelta since_session_start,
    base::TimeDelta idle_time) {
  return Do(base::BindRepeating(
      &BrowserFeaturePromoControllerTestBase::ResetSessionDataImpl,
      base::Unretained(this), since_session_start, idle_time, browser_view()));
}

void BrowserFeaturePromoControllerTestBase::ResetSessionDataImpl(
    base::TimeDelta since_session_start,
    base::TimeDelta idle_time,
    BrowserView* browser_view) {
  UserEducationSessionData session_data;
  session_data.start_time = kSessionStartTime;
  session_data.most_recent_active_time =
      kSessionStartTime + since_session_start;
  now_ = session_data.most_recent_active_time + idle_time;
  FeaturePromoPolicyData policy_data;
  test_util_ = std::make_unique<test::UserEducationSessionTestUtil>(
      UserEducationServiceFactory::GetForBrowserContext(
          browser_view->GetProfile())
          ->user_education_session_manager(),
      session_data, policy_data, session_data.most_recent_active_time, now_);
}

InteractiveBrowserTest::StepBuilder
BrowserFeaturePromoControllerTestBase::AdvanceTime(
    std::optional<base::TimeDelta> until_new_last_active,
    base::TimeDelta until_new_now,
    bool send_update) {
  return Do(base::BindRepeating(
      &BrowserFeaturePromoControllerTestBase::AdvanceTimeImpl,
      base::Unretained(this), until_new_last_active, until_new_now,
      send_update));
}

void BrowserFeaturePromoControllerTestBase::AdvanceTimeImpl(
    std::optional<base::TimeDelta> until_new_last_active,
    base::TimeDelta until_new_now,
    bool send_update) {
  const auto new_active_time =
      until_new_last_active ? std::make_optional(now_ + *until_new_last_active)
                            : std::nullopt;
  now_ = new_active_time.value_or(now_) + until_new_now;
  test_util_->SetNow(now_);
  if (new_active_time) {
    test_util_->UpdateLastActiveTime(*new_active_time, send_update);
  }
}

InteractiveBrowserTest::MultiStep
BrowserFeaturePromoControllerTestBase::MaybeShowPromo(
    FeaturePromoParams params,
    FeaturePromoResult expected,
    std::optional<base::TimeDelta> timeout_delta) {
  auto result =
      base::MakeRefCounted<base::RefCountedData<FeaturePromoResult>>();
  const std::string caller =
      base::StrCat({"MaybeShowPromo( ", params.feature->name, ", ",
                    base::ToString(expected), " )"});
  auto steps = Steps(WithElement(
      kBrowserViewElementId, [this, p = std::move(params), expected,
                              result](ui::TrackedElement* el) mutable {
        if (expected) {
          EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(*p.feature)))
              .WillOnce(Return(true));
        } else if (expected.failure() == FeaturePromoResult::kBlockedByConfig) {
          EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(*p.feature)))
              .WillOnce(Return(false));
        }

        ui::SafeElementReference el_ref(el);

        p.show_promo_result_callback = base::BindLambdaForTesting(
            [result, actual_callback = std::move(p.show_promo_result_callback),
             el_ref =
                 std::move(el_ref)](FeaturePromoResult actual_result) mutable {
              result->data = actual_result;
              if (actual_callback) {
                std::move(actual_callback).Run(actual_result);
              }
              auto* const el = el_ref.get();
              CHECK(el);
              ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                  el, test::kPromoShownEvent);
            });

        controller_->MaybeShowPromo(std::move(p), user_education_context_);
      }));

  if (timeout_delta) {
    steps.emplace_back(AdvanceTime(*timeout_delta + base::Milliseconds(500)));
  }

  steps += Steps(WaitForEvent(kBrowserViewElementId, test::kPromoShownEvent),
                 CheckResult([result]() { return result->data; }, expected));
  AddDescriptionPrefix(steps, caller);
  return steps;
}

InteractiveBrowserTest::MultiStep
BrowserFeaturePromoControllerTestBase::ClosePromo() {
  return Steps(
      PressButton(HelpBubbleView::kCloseButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting, true));
}

InteractiveBrowserTest::MultiStep
BrowserFeaturePromoControllerTestBase::AbortPromo() {
  return Steps(
      WithView(HelpBubbleView::kHelpBubbleElementIdForTesting,
               [](views::View* bubble) { bubble->GetWidget()->Close(); }),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting));
}

InteractiveBrowserTest::MultiStep
BrowserFeaturePromoControllerTestBase::ExpectShowingPromo(
    const base::Feature* feature) {
  if (feature) {
    return CheckPromoStatus(*feature, FeaturePromoStatus::kBubbleShowing);
  }
  return Steps(CheckResult(
      [this]() -> bool {
        return controller_->promo_bubble_for_testing() != nullptr;
      },
      false, "ExpectShowingPromo [none]"));
}

InteractiveBrowserTest::MultiStep
BrowserFeaturePromoControllerTestBase::CheckPromoStatus(
    const base::Feature& iph_feature,
    FeaturePromoStatus status) {
  return Steps(CheckResult(
      [this, &iph_feature]() -> FeaturePromoStatus {
        return controller_->GetPromoStatus(iph_feature);
      },
      status,
      base::StrCat({"CheckPromoStatus( ", iph_feature.name, ", ",
                    base::ToString(status), " )"})));
}

void BrowserFeaturePromoControllerTestBase::VerifyConstants() {
  CHECK_GT(kLessThanCooldown, kMoreThanNewSession);
  CHECK_LT(kMoreThanGracePeriod + kLessThanCooldown,
           features::GetLowPriorityCooldown());
  CHECK_LT(kMoreThanAbortCooldown + kMoreThanGracePeriod,
           features::GetSnoozeDuration());
}

FeaturePromoSpecification
BrowserFeaturePromoControllerTestBase::DefaultPromoSpecification(
    const base::Feature& feature) {
  return FeaturePromoSpecification::CreateForToastPromo(
      feature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP, IDS_OK,
      FeaturePromoSpecification::AcceleratorInfo());
}

UserEducationService*
BrowserFeaturePromoControllerTestBase::user_education_service() {
  return UserEducationServiceFactory::GetForBrowserContext(
      browser()->GetProfile());
}

UserEducationStorageService*
BrowserFeaturePromoControllerTestBase::storage_service() {
  return &user_education_service()->user_education_storage_service();
}

FeaturePromoRegistry* BrowserFeaturePromoControllerTestBase::registry() {
  return &user_education_service()->feature_promo_registry();
}

HelpBubbleFactoryRegistry*
BrowserFeaturePromoControllerTestBase::bubble_factory() {
  return &user_education_service()->help_bubble_factory_registry();
}

views::View* BrowserFeaturePromoControllerTestBase::GetAnchorView() {
  return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
      kToolbarAppMenuButtonElementId, browser_view()->GetElementContext());
}

ui::TrackedElement* BrowserFeaturePromoControllerTestBase::GetAnchorElement() {
  auto* const result =
      views::ElementTrackerViews::GetInstance()->GetElementForView(
          GetAnchorView());
  CHECK(result);
  return result;
}

void BrowserFeaturePromoControllerTestBase::OnCustomPromoAction(
    const base::Feature* feature,
    const UserEducationContextPtr& context,
    FeaturePromoHandle promo_handle) {
  ++custom_callback_count_;
  EXPECT_TRUE(promo_handle.is_valid());
  EXPECT_EQ(FeaturePromoStatus::kContinued,
            controller_->GetPromoStatus(*feature));
  EXPECT_EQ(BrowserElements::From(browser())->GetContext(),
            context->GetElementContext());
  promo_handle.Release();
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(*feature));
}

void BrowserFeaturePromoControllerTestBase::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(
                   &BrowserFeaturePromoControllerTestBase::MakeTestTracker));
}

std::unique_ptr<KeyedService>
BrowserFeaturePromoControllerTestBase::MakeTestTracker(
    content::BrowserContext* context) {
  auto tracker =
      std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

  // Allow other code to call into the tracker.
  EXPECT_CALL(*tracker, IsInFeatureTestMode).WillRepeatedly(Return(true));
  EXPECT_CALL(*tracker, IsInitialized).WillRepeatedly(Return(true));
  EXPECT_CALL(*tracker, NotifyEvent(testing::_)).Times(testing::AnyNumber());
  EXPECT_CALL(*tracker, ShouldTriggerHelpUI(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*tracker, ListEvents)
      .WillRepeatedly(Return(feature_engagement::Tracker::EventList()));

  return tracker;
}

}  // namespace user_education
