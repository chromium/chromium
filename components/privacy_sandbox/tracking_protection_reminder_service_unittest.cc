// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_reminder_service.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tracking_protection_prefs.h"
#include "tracking_protection_reminder_service.h"

namespace privacy_sandbox {

class MockTrackingProtectionReminderObserver
    : public TrackingProtectionReminderService::Observer {
 public:
  MOCK_METHOD(
      void,
      OnTrackingProtectionReminderStatusChanged,
      (tracking_protection::TrackingProtectionReminderStatus reminder_status),
      (override));
};

class TrackingProtectionReminderServiceTest : public testing::Test {
 public:
  TrackingProtectionReminderServiceTest() {
    tracking_protection::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(
            prefs(), version_info::Channel::DEV);
    tracking_protection_reminder_service_ =
        std::make_unique<TrackingProtectionReminderService>(
            prefs(), onboarding_service());

    // Default to profiles not being apart of Mode B.
    reminder_service()->is_mode_b_user_ = false;
  }

  void TearDown() override {
    tracking_protection_reminder_service_ = nullptr;
    tracking_protection_onboarding_service_ = nullptr;
  }

  TrackingProtectionOnboarding* onboarding_service() {
    return tracking_protection_onboarding_service_.get();
  }

  TrackingProtectionReminderService* reminder_service() {
    return tracking_protection_reminder_service_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  void RunOnboardingLogic(bool is_silent_onboarding) {
    if (is_silent_onboarding) {
      reminder_service()->OnTrackingProtectionSilentOnboardingUpdated(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kOnboarded);
    } else {
      reminder_service()->OnTrackingProtectionOnboardingUpdated(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
    }
  }

  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionReminderService>
      tracking_protection_reminder_service_;
  std::unique_ptr<TrackingProtectionOnboarding>
      tracking_protection_onboarding_service_;
  base::test::ScopedFeatureList feature_list_;
};

class TrackingProtectionReminderServiceReminderTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  TrackingProtectionReminderServiceReminderTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kTrackingProtectionReminder, {{"is-silent-reminder", "false"}}}};
  }
};

TEST_F(TrackingProtectionReminderServiceReminderTest,
       UpdatesStatusToPendingReminder) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  // Simulate a regular onboarding experience.
  RunOnboardingLogic(/*is_silent_onboarding=*/false);

  // Expect this profile to see a regular reminder.
  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));
}

TEST_F(TrackingProtectionReminderServiceReminderTest,
       EmitsOnStatusChangedObservable) {
  MockTrackingProtectionReminderObserver observer;
  tracking_protection_reminder_service_->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionReminderStatusChanged(
                  tracking_protection::TrackingProtectionReminderStatus::
                      kPendingReminder));

  // Check that the status is not initialized.
  EXPECT_EQ(prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
            static_cast<int>(
                tracking_protection::TrackingProtectionReminderStatus::kUnset));

  // Simulate a regular onboarding experience.
  RunOnboardingLogic(/*is_silent_onboarding=*/false);

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionReminderServiceReminderTest, UpdatesStatusToInvalid) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  // Simulate a silent onboarding.
  RunOnboardingLogic(/*is_silent_onboarding=*/true);

  // We shouldn't show reminders after a silent onboarding, instead we should
  // end up in an invalid state.
  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kInvalid));
}

TEST_P(TrackingProtectionReminderServiceReminderTest,
       DoesNotOverwriteExistingStatus) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kFeatureDisabledSkipped));

  RunOnboardingLogic(/*is_silent_onboarding=*/GetParam());

  // Check that the status did not change.
  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kFeatureDisabledSkipped));
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionReminderServiceReminderTest,
                         TrackingProtectionReminderServiceReminderTest,
                         /*is_silent_onboarding=*/testing::Bool());

class TrackingProtectionReminderServiceSilentReminderTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  TrackingProtectionReminderServiceSilentReminderTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kTrackingProtectionReminder, {{"is-silent-reminder", "true"}}}};
  }
};

TEST_P(TrackingProtectionReminderServiceSilentReminderTest,
       SetsStatusToPending) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  RunOnboardingLogic(/*is_silent_onboarding=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));
}

TEST_P(TrackingProtectionReminderServiceSilentReminderTest,
       EmitsOnStatusChangedObservable) {
  MockTrackingProtectionReminderObserver observer;
  tracking_protection_reminder_service_->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionReminderStatusChanged(
                  tracking_protection::TrackingProtectionReminderStatus::
                      kPendingReminder));

  // Check that the status is not initialized.
  EXPECT_EQ(prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
            static_cast<int>(
                tracking_protection::TrackingProtectionReminderStatus::kUnset));

  RunOnboardingLogic(/*is_silent_onboarding=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionReminderServiceSilentReminderTest,
                         TrackingProtectionReminderServiceSilentReminderTest,
                         /*is_silent_onboarding=*/testing::Bool());

class TrackingProtectionReminderServiceDisabledReminderFeatureTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  TrackingProtectionReminderServiceDisabledReminderFeatureTest() {
    feature_list_.InitWithFeaturesAndParameters({}, {});
  }
};

TEST_P(TrackingProtectionReminderServiceDisabledReminderFeatureTest,
       SetsStatusToSkipped) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  RunOnboardingLogic(/*is_silent_onboarding=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kFeatureDisabledSkipped));
}

TEST_P(TrackingProtectionReminderServiceDisabledReminderFeatureTest,
       EmitsOnStatusChangedObservable) {
  MockTrackingProtectionReminderObserver observer;
  tracking_protection_reminder_service_->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionReminderStatusChanged(
                  tracking_protection::TrackingProtectionReminderStatus::
                      kFeatureDisabledSkipped));

  // Check that the status is not initialized.
  EXPECT_EQ(prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
            static_cast<int>(
                tracking_protection::TrackingProtectionReminderStatus::kUnset));

  RunOnboardingLogic(/*is_silent_onboarding=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kFeatureDisabledSkipped));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionReminderServiceDisabledReminderFeatureTest,
    TrackingProtectionReminderServiceDisabledReminderFeatureTest,
    /*is_silent_onboarding=*/testing::Bool());

class TrackingProtectionReminderServiceModeBEnabledTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  TrackingProtectionReminderServiceModeBEnabledTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

  void SetIsModeBUser(bool is_mode_b_user) {
    reminder_service()->is_mode_b_user_ = is_mode_b_user;
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kTrackingProtectionReminder, {{}}}};
  }
};

TEST_P(TrackingProtectionReminderServiceModeBEnabledTest,
       ExcludesModeBUsersAfterOnboarding) {
  // Check that the status is not initialized.
  EXPECT_EQ(prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
            static_cast<int>(
                tracking_protection::TrackingProtectionReminderStatus::kUnset));

  SetIsModeBUser(true);
  RunOnboardingLogic(/*is_silent_onboarding=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kModeBUserSkipped));
}

TEST_P(TrackingProtectionReminderServiceModeBEnabledTest,
       EmitsOnStatusChangedObservable) {
  MockTrackingProtectionReminderObserver observer;
  tracking_protection_reminder_service_->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionReminderStatusChanged(
                  tracking_protection::TrackingProtectionReminderStatus::
                      kModeBUserSkipped));

  // Check that the status is not initialized.
  EXPECT_EQ(prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
            static_cast<int>(
                tracking_protection::TrackingProtectionReminderStatus::kUnset));

  SetIsModeBUser(true);
  RunOnboardingLogic(/*is_silent_onboarding=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kModeBUserSkipped));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionReminderServiceModeBEnabledTest,
                         TrackingProtectionReminderServiceModeBEnabledTest,
                         /*is_silent_onboarding=*/testing::Bool());

class TrackingProtectionReminderServiceObserverTest
    : public TrackingProtectionReminderServiceTest {
 public:
  TrackingProtectionReminderServiceObserverTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kTrackingProtectionReminder, {{}}}};
  }
};

TEST_F(TrackingProtectionReminderServiceObserverTest,
       EmitsOnStatusChangedObservableForInvalidStatus) {
  MockTrackingProtectionReminderObserver observer;
  tracking_protection_reminder_service_->AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnTrackingProtectionReminderStatusChanged(
          tracking_protection::TrackingProtectionReminderStatus::kInvalid));

  // Check that the status is not initialized.
  EXPECT_EQ(prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
            static_cast<int>(
                tracking_protection::TrackingProtectionReminderStatus::kUnset));

  RunOnboardingLogic(/*is_silent_onboarding=*/true);

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kInvalid));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace privacy_sandbox
