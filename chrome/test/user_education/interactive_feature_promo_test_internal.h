// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_INTERNAL_H_
#define CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_INTERNAL_H_

#include <map>
#include <memory>
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test_internal.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/test/feature_promo_session_test_util.h"
#include "content/public/browser/browser_context.h"

namespace internal {

class InteractiveFeaturePromoTestPrivate
    : public InteractiveBrowserTestPrivate,
      public InteractiveFeaturePromoTestCommon,
      public ProfileObserver {
 public:
  InteractiveFeaturePromoTestPrivate(
      std::unique_ptr<InteractionTestUtilBrowser> test_util,
      TrackerMode tracker_mode,
      ClockMode clock_mode,
      InitialSessionState initial_session_state);
  ~InteractiveFeaturePromoTestPrivate() override;

  // InteractiveBrowserTestPrivate:
  void DoTestTearDown() override;

  // Returns the mock tracker for `browser` if in `UseMockTracker` mode.
  MockTracker* GetMockTrackerFor(Browser* browser);

  // Implementation for `InteractiveFeaturePromoTestApi` methods.
  void AdvanceTime(NewTime new_time);
  void SetLastActive(NewTime time);

  // Waits for the tracker to be initialized if the appropriate tracker mode is
  // set.
  void MaybeWaitForTrackerInitialization(Browser* browser);

 private:
  struct ProfileData {
    ProfileData();
    ProfileData(ProfileData&&) noexcept;
    ~ProfileData();

    raw_ptr<MockTracker> mock_tracker = nullptr;
    std::unique_ptr<user_education::test::FeaturePromoSessionTestUtil>
        test_util;
  };

  // Called when a new `Profile` is created; allows overriding some default
  // behaviors.
  void CreateServicesCallback(content::BrowserContext* context);

  // Called when a tracker will be generated and the test is in `UseMockTracker`
  // mode.
  static std::unique_ptr<KeyedService> CreateMockTracker(
      base::WeakPtr<InteractiveFeaturePromoTestPrivate> ptr,
      content::BrowserContext* context);

  // Called when a `UserEducationService` object will be generated.
  static std::unique_ptr<KeyedService> CreateUserEducationService(
      base::WeakPtr<InteractiveFeaturePromoTestPrivate> ptr,
      content::BrowserContext* context);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  const TrackerMode tracker_mode_;
  const InitialSessionState initial_session_state_;
  std::optional<base::Time> test_time_;
  std::map<Profile*, ProfileData> profile_data_;
  feature_engagement::test::ScopedIphFeatureList feature_list_;
  user_education::FeaturePromoControllerCommon::TestLock activation_lock_;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<InteractiveFeaturePromoTestPrivate> weak_ptr_factory_{
      this};
};

}  // namespace internal

#endif  // CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_INTERNAL_H_
