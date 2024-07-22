// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_reminder_service.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/mock_tracking_protection_onboarding_delegate.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tracking_protection_prefs.h"
#include "tracking_protection_reminder_service.h"

namespace privacy_sandbox {

using NoticeType = privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using SurfaceType = privacy_sandbox::TrackingProtectionOnboarding::SurfaceType;

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
  TrackingProtectionReminderServiceTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    tracking_protection::RegisterProfilePrefs(prefs()->registry());
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    auto delegate =
        std::make_unique<MockTrackingProtectionOnboardingDelegate>();
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});

    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(
            std::move(delegate), prefs(), version_info::Channel::DEV);
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
  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {};
  }

  // TODO(crbug.com/353703170): Handle onboarding for the 3PCD Notice.
  void ShowOnboardingNotice(bool is_silent) {
    if (is_silent) {
      onboarding_service()->MaybeMarkModeBSilentEligible();
      onboarding_service()->NoticeShown(SurfaceType::kDesktop,
                                        NoticeType::kModeBSilentOnboarding);
    } else {
      onboarding_service()->MaybeMarkModeBEligible();
      onboarding_service()->NoticeShown(SurfaceType::kDesktop,
                                        NoticeType::kModeBOnboarding);
    }
  }
  void CallOnboardingObserver(bool is_silent) {
    if (is_silent) {
      reminder_service()->OnTrackingProtectionSilentOnboardingUpdated(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kOnboarded);
    } else {
      reminder_service()->OnTrackingProtectionOnboardingUpdated(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
    }
  }

  base::test::TaskEnvironment task_env_;
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
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionReminder,
             {{"is-silent-reminder", "false"}, {"reminder-delay", "7d"}}}};
  }
};

TEST_F(TrackingProtectionReminderServiceReminderTest,
       UpdatesStatusToPendingReminder) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  // Simulate a regular onboarding experience.
  CallOnboardingObserver(/*is_silent=*/false);

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
  CallOnboardingObserver(/*is_silent=*/false);

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
  CallOnboardingObserver(/*is_silent=*/true);

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

  CallOnboardingObserver(/*is_silent=*/GetParam());

  // Check that the status did not change.
  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kFeatureDisabledSkipped));
}

TEST_F(TrackingProtectionReminderServiceReminderTest,
       ExpectsActiveReminderToBeExperienced) {
  // The only valid case to see a active reminder would be on a non-silent
  // onboarding.
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);

  task_env_.FastForwardBy(base::Days(7));
  EXPECT_EQ(reminder_service()->GetReminderType(), ReminderType::kActive);
}

TEST_F(TrackingProtectionReminderServiceReminderTest,
       ExpectsNoReminderExperienceWhenOnboardingTimestampsNotSet) {
  // By not calling `ShowOnboardingNotice` we will not be setting the
  // timestamps.
  CallOnboardingObserver(/*is_silent=*/false);

  // Since the onboarding timestamp won't be set, we should always return
  // `kNone`.
  EXPECT_EQ(reminder_service()->GetReminderType(), ReminderType::kNone);
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionReminderServiceReminderTest,
                         TrackingProtectionReminderServiceReminderTest,
                         /*is_silent=*/testing::Bool());

class TrackingProtectionReminderServiceSilentReminderTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionReminder,
             {{"is-silent-reminder", "true"}, {"reminder-delay", "7d"}}}};
  }
};

TEST_P(TrackingProtectionReminderServiceSilentReminderTest,
       SetsStatusToPending) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  CallOnboardingObserver(/*is_silent=*/GetParam());

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

  CallOnboardingObserver(/*is_silent=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_P(TrackingProtectionReminderServiceSilentReminderTest,
       ExpectsSilentReminderToBeExperienced) {
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());

  task_env_.FastForwardBy(base::Days(7));
  EXPECT_EQ(reminder_service()->GetReminderType(), ReminderType::kSilent);
}

TEST_P(TrackingProtectionReminderServiceSilentReminderTest,
       SilentReminderExperienced) {
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  task_env_.FastForwardBy(base::Days(7));
  EXPECT_EQ(reminder_service()->GetReminderType(), ReminderType::kSilent);
  // Check that there does not exists a existing timestamp.
  EXPECT_EQ(
      reminder_service()->GetReminderNoticeData(
          /*surface_type=*/TrackingProtectionOnboarding::SurfaceType::kDesktop),
      std::nullopt);

  // Simulate a successful silent reminder
  reminder_service()->OnReminderExperienced(
      /*surface_type=*/TrackingProtectionOnboarding::SurfaceType::kDesktop);

  // Confirm that the reminder timestamp was logged.
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    /*surface_type=*/TrackingProtectionOnboarding::SurfaceType::
                        kDesktop)
                ->notice_first_shown,
            base::Time::Now());
  EXPECT_EQ(reminder_service()->GetReminderStatus(),
            tracking_protection::TrackingProtectionReminderStatus::
                kExperiencedReminder);
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionReminderServiceSilentReminderTest,
                         TrackingProtectionReminderServiceSilentReminderTest,
                         /*is_silent=*/testing::Bool());

class TrackingProtectionReminderServiceDisabledReminderFeatureTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {};

TEST_P(TrackingProtectionReminderServiceDisabledReminderFeatureTest,
       SetsStatusToSkipped) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  CallOnboardingObserver(/*is_silent=*/GetParam());

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

  CallOnboardingObserver(/*is_silent=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kFeatureDisabledSkipped));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_P(TrackingProtectionReminderServiceDisabledReminderFeatureTest,
       ExpectNoReminderExperience) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kUnset));

  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kFeatureDisabledSkipped));
  // Since the status != `kPendingReminder` we should always expect `kNone`.
  EXPECT_EQ(reminder_service()->GetReminderType(), ReminderType::kNone);
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionReminderServiceDisabledReminderFeatureTest,
    TrackingProtectionReminderServiceDisabledReminderFeatureTest,
    /*is_silent=*/testing::Bool());

class TrackingProtectionReminderServiceModeBEnabledTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetIsModeBUser(bool is_mode_b_user) {
    reminder_service()->is_mode_b_user_ = is_mode_b_user;
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
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
  CallOnboardingObserver(/*is_silent=*/GetParam());

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
  CallOnboardingObserver(/*is_silent=*/GetParam());

  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kModeBUserSkipped));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionReminderServiceModeBEnabledTest,
                         TrackingProtectionReminderServiceModeBEnabledTest,
                         /*is_silent=*/testing::Bool());

class TrackingProtectionReminderServiceObserverTest
    : public TrackingProtectionReminderServiceTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionReminder, {{"is_silent_reminder", "false"}}}};
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

  CallOnboardingObserver(/*is_silent=*/true);

  // Status should be `kInvalid``since we were silently onboarded and we expect
  // a non-silent reminder.
  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(
          tracking_protection::TrackingProtectionReminderStatus::kInvalid));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

class TrackingProtectionReminderServiceReminderDelayTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<bool> {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionReminder,
             {{"is-silent-reminder", GetParam() ? "true" : "false"},
              {"reminder-delay", "7d"}}}};
  }
};

TEST_P(TrackingProtectionReminderServiceReminderDelayTest,
       ReminderDelayNotMetNoReminderToBeExperienced) {
  // Test only the non-silent route to avoid the invalid case of silent
  // onboarding + active reminder.
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);

  // Expected delay not met, we should always return `kNone`.
  EXPECT_EQ(reminder_service()->GetReminderType(), ReminderType::kNone);
}

TEST_P(TrackingProtectionReminderServiceReminderDelayTest,
       ExpectReminderToBeExperienced) {
  // Test only the non-silent route to avoid the invalid case of silent
  // onboarding + active reminder.
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);

  // Fast forward to meet the expected reminder delay requirement.
  task_env_.FastForwardBy(base::Days(7));
  EXPECT_EQ(reminder_service()->GetReminderType(),
            GetParam() ? ReminderType::kSilent : ReminderType::kActive);
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionReminderServiceReminderDelayTest,
                         TrackingProtectionReminderServiceReminderDelayTest,
                         /*is_silent_reminder=*/testing::Bool());

class TrackingProtectionReminderServiceUnsetReminderDelayTest
    : public TrackingProtectionReminderServiceTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionReminder, {{}}}};
  }
};

TEST_F(TrackingProtectionReminderServiceUnsetReminderDelayTest,
       ReminderDelaySetToDefaultValue) {
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);

  // Confirm that the reminder delay defaults to `TimeDelta::Max()`.
  EXPECT_EQ(privacy_sandbox::kTrackingProtectionReminderDelay.Get(),
            base::TimeDelta::Max());
  // Fast forward some amount of time to ensure the default doesn't cross the
  // threshold.
  task_env_.FastForwardBy(base::Days(28));
  // No reminder should be experienced since the threshold is unreachable.
  EXPECT_EQ(reminder_service()->GetReminderType(), ReminderType::kNone);
}

class TrackingProtectionReminderServiceOnReminderExperiencedTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<
          TrackingProtectionOnboarding::SurfaceType> {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionReminder, {{}}}};
  }
};

TEST_P(TrackingProtectionReminderServiceOnReminderExperiencedTest,
       ReminderShownAndLogged) {
  // Reminder status will only update if called with status = `kPendingReminder`
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));
  // Check that the experienced timestamp not been set.
  EXPECT_EQ(reminder_service()->GetReminderNoticeData(
                /*surface_type=*/GetParam()),
            std::nullopt);

  reminder_service()->OnReminderExperienced(
      /*surface_type=*/GetParam());

  // Confirm that the status was updated to `kExperiencedReminder`.
  EXPECT_EQ(
      prefs()->GetInteger(prefs::kTrackingProtectionReminderStatus),
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kExperiencedReminder));
  // Confirm that the reminder timestamp was correctly recorded.
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    /*surface_type=*/GetParam())
                ->notice_first_shown,
            base::Time::Now());
}

TEST_F(TrackingProtectionReminderServiceOnReminderExperiencedTest,
       CrashWhenSurfaceTypeIsCCT) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));
  EXPECT_DEATH_IF_SUPPORTED(
      reminder_service()->OnReminderExperienced(
          TrackingProtectionOnboarding::SurfaceType::kAGACCT),
      "");
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionReminderServiceOnReminderExperiencedTest,
    TrackingProtectionReminderServiceOnReminderExperiencedTest,
    /*surface_type=*/
    testing::ValuesIn({TrackingProtectionOnboarding::SurfaceType::kDesktop,
                       TrackingProtectionOnboarding::SurfaceType::kBrApp}));

class TrackingProtectionReminderServiceOnReminderActionTakenTest
    : public TrackingProtectionReminderServiceTest,
      public testing::WithParamInterface<
          TrackingProtectionOnboarding::SurfaceType> {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionReminder, {{"silent-reminder", "false"}}}};
  }
};

TEST_P(TrackingProtectionReminderServiceOnReminderActionTakenTest,
       ReminderRecordedOnClosed) {
  EXPECT_EQ(reminder_service()->GetReminderNoticeData(
                /*surface_type=*/GetParam()),
            std::nullopt);
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kPendingReminder));

  // Simulate the reminder being shown.
  reminder_service()->OnReminderExperienced(
      /*surface_type=*/GetParam());
  // Simulate the reminder timing out.
  task_env_.FastForwardBy(base::Seconds(10));
  reminder_service()->OnReminderActionTaken(NoticeActionTaken::kOther,
                                            base::Time::Now(),
                                            /*surface_type=*/GetParam());

  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    /*surface_type=*/GetParam())
                ->notice_shown_duration,
            base::Seconds(10));
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    /*surface_type=*/GetParam())
                ->notice_action_taken,
            NoticeActionTaken::kOther);
}

TEST_F(TrackingProtectionReminderServiceOnReminderActionTakenTest,
       CrashWhenSurfaceTypeIsCCT) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(tracking_protection::TrackingProtectionReminderStatus::
                           kExperiencedReminder));
  EXPECT_DEATH_IF_SUPPORTED(
      reminder_service()->OnReminderActionTaken(
          NoticeActionTaken::kOther, base::Time::Now(),
          TrackingProtectionOnboarding::SurfaceType::kAGACCT),
      "");
  EXPECT_DEATH_IF_SUPPORTED(
      reminder_service()->GetReminderNoticeData(
          TrackingProtectionOnboarding::SurfaceType::kAGACCT),
      "");
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionReminderServiceOnReminderActionTakenTest,
    TrackingProtectionReminderServiceOnReminderActionTakenTest,
    /*surface_type=*/
    testing::ValuesIn({TrackingProtectionOnboarding::SurfaceType::kDesktop,
                       TrackingProtectionOnboarding::SurfaceType::kBrApp}));

}  // namespace privacy_sandbox
