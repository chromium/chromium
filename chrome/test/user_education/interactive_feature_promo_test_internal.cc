// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/user_education/interactive_feature_promo_test_internal.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test_internal.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/test/feature_promo_session_test_util.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace internal {

namespace {

std::optional<base::Time> CalculateNewTime(
    base::Time now,
    InteractiveFeaturePromoTestCommon::NewTime time) {
  if (std::holds_alternative<std::nullopt_t>(time)) {
    return std::nullopt;
  }
  if (const auto* const rel_time = std::get_if<base::TimeDelta>(&time)) {
    return now + *rel_time;
  }
  return std::get<base::Time>(time);
}

}  // namespace

InteractiveFeaturePromoTestPrivate::ProfileData::ProfileData() = default;
InteractiveFeaturePromoTestPrivate::ProfileData::ProfileData(
    ProfileData&&) noexcept = default;
InteractiveFeaturePromoTestPrivate::ProfileData::~ProfileData() = default;

InteractiveFeaturePromoTestPrivate::InteractiveFeaturePromoTestPrivate(
    std::unique_ptr<InteractionTestUtilBrowser> test_util,
    TrackerMode tracker_mode,
    ClockMode clock_mode,
    InitialSessionState initial_session_state)
    : InteractiveBrowserTestPrivate(std::move(test_util)),
      tracker_mode_(std::move(tracker_mode)),
      initial_session_state_(initial_session_state) {
  test_time_ = clock_mode == ClockMode::kUseTestClock
                   ? std::make_optional(base::Time::Now())
                   : std::nullopt;
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &InteractiveFeaturePromoTestPrivate::CreateServicesCallback,
              base::Unretained(this)));
  activation_lock_ = user_education::FeaturePromoControllerCommon::
      BlockActiveWindowCheckForTesting();
  if (const auto* const allow_promos =
          std::get_if<UseDefaultTrackerAllowingPromos>(&tracker_mode_)) {
    feature_list_.InitAndEnableFeatures(allow_promos->features);
  }
}

InteractiveFeaturePromoTestPrivate::~InteractiveFeaturePromoTestPrivate() =
    default;

void InteractiveFeaturePromoTestPrivate::DoTestTearDown() {
  profile_observations_.RemoveAllObservations();
  profile_data_.clear();
  activation_lock_.reset();
  InteractiveBrowserTestPrivate::DoTestTearDown();
}

InteractiveFeaturePromoTestPrivate::MockTracker*
InteractiveFeaturePromoTestPrivate::GetMockTrackerFor(Browser* browser) {
  auto* const data = base::FindOrNull(profile_data_, browser->profile());
  return data ? data->mock_tracker : nullptr;
}

void InteractiveFeaturePromoTestPrivate::AdvanceTime(NewTime new_time) {
  CHECK(test_time_.has_value());
  test_time_ = CalculateNewTime(*test_time_, new_time);
  for (auto& [profile, data] : profile_data_) {
    if (data.test_util) {
      data.test_util->SetNow(*test_time_);
    }
  }
}

void InteractiveFeaturePromoTestPrivate::SetLastActive(NewTime time) {
  CHECK(test_time_.has_value());
  const auto last_active_time =
      CalculateNewTime(test_time_.value_or(base::Time::Now()), time);
  for (auto& [profile, data] : profile_data_) {
    if (data.test_util) {
      data.test_util->UpdateLastActiveTime(last_active_time,
                                           last_active_time.has_value());
    }
  }
}

void InteractiveFeaturePromoTestPrivate::MaybeWaitForTrackerInitialization(
    Browser* browser) {
  const auto* const mode =
      std::get_if<UseDefaultTrackerAllowingPromos>(&tracker_mode_);
  if (mode && mode->initialization_mode ==
                  TrackerInitializationMode::kWaitForMainBrowser) {
    auto* const tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser->profile());
    ASSERT_NE(nullptr, tracker);
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    tracker->AddOnInitializedCallback(
        base::BindLambdaForTesting([&run_loop](bool initialized) {
          run_loop.Quit();
          ASSERT_TRUE(initialized);
        }));
    run_loop.Run();
  }
}

void InteractiveFeaturePromoTestPrivate::CreateServicesCallback(
    content::BrowserContext* context) {
  auto* const profile = Profile::FromBrowserContext(context);
  if (base::Contains(profile_data_, profile)) {
    return;
  }
  profile_data_.emplace(profile, ProfileData());
  profile_observations_.AddObservation(profile);
  if (std::holds_alternative<UseMockTracker>(tracker_mode_)) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &InteractiveFeaturePromoTestPrivate::CreateMockTracker,
                     weak_ptr_factory_.GetWeakPtr()));
  }
  UserEducationServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          &InteractiveFeaturePromoTestPrivate::CreateUserEducationService,
          weak_ptr_factory_.GetWeakPtr()));
}

// static
std::unique_ptr<KeyedService>
InteractiveFeaturePromoTestPrivate::CreateMockTracker(
    base::WeakPtr<InteractiveFeaturePromoTestPrivate> ptr,
    content::BrowserContext* context) {
  auto mock_tracker =
      std::make_unique<InteractiveFeaturePromoTestPrivate::MockTracker>();

  // Allow an unlimited number of calls to these methods.
  EXPECT_CALL(*mock_tracker, IsInitialized)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_tracker, WouldTriggerHelpUI)
      .WillRepeatedly(testing::Return(true));

  // Because some features are enabled by default, ensure that anything other
  // than the specific feature being tested is rejected.
  EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI)
      .WillRepeatedly(testing::Return(false));

  if (ptr) {
    auto* const data = base::FindOrNull(ptr->profile_data_,
                                        Profile::FromBrowserContext(context));
    CHECK(data);
    data->mock_tracker = mock_tracker.get();
  }

  return mock_tracker;
}

std::unique_ptr<KeyedService>
InteractiveFeaturePromoTestPrivate::CreateUserEducationService(
    base::WeakPtr<InteractiveFeaturePromoTestPrivate> ptr,
    content::BrowserContext* context) {
  auto service =
      UserEducationServiceFactory::BuildServiceInstanceForBrowserContextImpl(
          context, /*disable_idle_polling=*/true);

  if (ptr) {
    auto* const profile = Profile::FromBrowserContext(context);
    auto* profile_data = base::FindOrNull(ptr->profile_data_, profile);
    CHECK(profile_data);

    user_education::FeaturePromoSessionData session_data;
    base::Time now = ptr->test_time_.value_or(base::Time::Now());
    switch (ptr->initial_session_state_) {
      case InitialSessionState::kInsideGracePeriod:
        session_data.start_time =
            now - user_education::features::GetSessionStartGracePeriod() / 2;
        session_data.most_recent_active_time = now;
        break;
      case InitialSessionState::kOutsideGracePeriod:
        session_data.start_time =
            now - (user_education::features::GetSessionStartGracePeriod() +
                   base::Minutes(5));
        session_data.most_recent_active_time = now;
        break;
    }

    profile_data->test_util =
        std::make_unique<user_education::test::FeaturePromoSessionTestUtil>(
            service->feature_promo_session_manager(), session_data,
            user_education::FeaturePromoPolicyData(), now, ptr->test_time_);
  }

  return service;
}

void InteractiveFeaturePromoTestPrivate::OnProfileWillBeDestroyed(
    Profile* profile) {
  profile_data_.erase(profile);
  profile_observations_.RemoveObservation(profile);
}

}  // namespace internal
