// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"
#include <memory>
#include <utility>
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

class MockTrackingProtectionSettingsObserver
    : public TrackingProtectionSettingsObserver {
 public:
  MOCK_METHOD(void, OnDoNotTrackEnabledChanged, (), (override));
  MOCK_METHOD(void, OnBlockAllThirdPartyCookiesChanged, (), (override));
  MOCK_METHOD(void, OnTrackingProtection3pcdChanged, (), (override));
};

class TrackingProtectionSettingsTest : public testing::Test {
 public:
  TrackingProtectionSettingsTest() {
    RegisterProfilePrefs(prefs()->registry());
    onboarding_service_ = std::make_unique<TrackingProtectionOnboarding>(
        &prefs_, version_info::Channel::UNKNOWN);
  }

  void SetUp() override {
    tracking_protection_settings_ =
        std::make_unique<TrackingProtectionSettings>(
            prefs(), onboarding_service_.get(), /*is_incognito=*/false);
  }

  TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_.get();
  }

  TrackingProtectionOnboarding* onboarding_service() {
    return onboarding_service_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionOnboarding> onboarding_service_;
  std::unique_ptr<TrackingProtectionSettings> tracking_protection_settings_;
};

TEST_F(TrackingProtectionSettingsTest, ReturnsDoNotTrackStatus) {
  EXPECT_FALSE(tracking_protection_settings()->IsDoNotTrackEnabled());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, true);
  EXPECT_TRUE(tracking_protection_settings()->IsDoNotTrackEnabled());
}

TEST_F(TrackingProtectionSettingsTest,
       DisablesTrackingProtection3pcdWhenEnterpriseControlEnabled) {
  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, false);
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_TRUE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());

  prefs()->SetManagedPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                          std::make_unique<base::Value>(false));
  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsTrackingProtection3pcdStatus) {
  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_TRUE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
}

TEST_F(TrackingProtectionSettingsTest, AreAll3pcBlockedTrueInIncognito) {
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_TRUE(
      TrackingProtectionSettings(prefs(), nullptr, /*is_incognito=*/true)
          .AreAllThirdPartyCookiesBlocked());
  EXPECT_FALSE(
      TrackingProtectionSettings(prefs(), nullptr, /*is_incognito=*/false)
          .AreAllThirdPartyCookiesBlocked());
}

TEST_F(TrackingProtectionSettingsTest, AreAll3pcBlockedFalseOutside3pcd) {
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  EXPECT_FALSE(
      TrackingProtectionSettings(prefs(), nullptr, /*is_incognito=*/false)
          .AreAllThirdPartyCookiesBlocked());
}

TEST_F(TrackingProtectionSettingsTest,
       SetsTrackingProtection3pcdStatusUsingOnboardingService) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  EXPECT_CALL(observer, OnTrackingProtection3pcdChanged());

  tracking_protection_settings()->OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  EXPECT_CALL(observer, OnTrackingProtection3pcdChanged());

  tracking_protection_settings()->OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus::kOffboarded);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
}

TEST_F(TrackingProtectionSettingsTest, CorrectlyCallsObserversForDoNotTrack) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnDoNotTrackEnabledChanged());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnDoNotTrackEnabledChanged());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSettingsTest, CorrectlyCallsObserversForBlockAll3pc) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

class TrackingProtectionSettingsStartupTest
    : public TrackingProtectionSettingsTest {
 public:
  void SetUp() override {
    // Profiles gets onboarded before the settings service is started.
    onboarding_service()->MaybeMarkEligible();
    onboarding_service()->NoticeShown(
        TrackingProtectionOnboarding::NoticeType::kOnboarding);
    TrackingProtectionSettingsTest::SetUp();
  }
};

TEST_F(TrackingProtectionSettingsStartupTest,
       SetsTrackingProtection3pcdStatusUsingOnboardingServiceOnStartup) {
  EXPECT_TRUE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
}

}  // namespace
}  // namespace privacy_sandbox
