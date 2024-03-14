// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"
#include <memory>
#include <utility>
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
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
  MOCK_METHOD(void, OnFingerprintingProtectionEnabledChanged, (), (override));
  MOCK_METHOD(void, OnIpProtectionEnabledChanged, (), (override));
  MOCK_METHOD(void, OnBlockAllThirdPartyCookiesChanged, (), (override));
  MOCK_METHOD(void, OnTrackingProtection3pcdChanged, (), (override));
};

class TrackingProtectionSettingsTest : public testing::Test {
 public:
  TrackingProtectionSettingsTest() {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    RegisterProfilePrefs(prefs()->registry());
    onboarding_service_ = std::make_unique<TrackingProtectionOnboarding>(
        &prefs_, version_info::Channel::UNKNOWN);
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kIpProtectionV1,
         privacy_sandbox::kFingerprintingProtectionSetting,
         privacy_sandbox::kTrackingProtectionSettingsLaunch},
        {});
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

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TrackingProtectionOnboarding> onboarding_service_;
  std::unique_ptr<TrackingProtectionSettings> tracking_protection_settings_;
};

// Gets prefs

TEST_F(TrackingProtectionSettingsTest, ReturnsDoNotTrackStatus) {
  EXPECT_FALSE(tracking_protection_settings()->IsDoNotTrackEnabled());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, true);
  EXPECT_TRUE(tracking_protection_settings()->IsDoNotTrackEnabled());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsIpProtectionStatus) {
  EXPECT_FALSE(tracking_protection_settings()->IsIpProtectionEnabled());
  prefs()->SetBoolean(prefs::kIpProtectionEnabled, true);
  EXPECT_TRUE(tracking_protection_settings()->IsIpProtectionEnabled());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsFingerprintingProtectionStatus) {
  EXPECT_FALSE(
      tracking_protection_settings()->IsFingerprintingProtectionEnabled());
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled, true);
  EXPECT_TRUE(
      tracking_protection_settings()->IsFingerprintingProtectionEnabled());
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

// Sets prefs

TEST_F(TrackingProtectionSettingsTest,
       SetsTrackingProtection3pcdStatusAndBlockAllPrefUsingOnboardingService) {
  // The user has chosen to block all 3PC.
  prefs()->SetInteger(prefs::kCookieControlsMode, 1 /* BlockThirdParty */);
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
  EXPECT_FALSE(
      tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
  EXPECT_CALL(observer, OnTrackingProtection3pcdChanged());
  // Called on changes to TrackingProtection pref and BlockAll3pc pref.
  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged()).Times(2);

  tracking_protection_settings()->OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
  EXPECT_TRUE(tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  EXPECT_CALL(observer, OnTrackingProtection3pcdChanged());

  tracking_protection_settings()->OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus::kOffboarded);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
  EXPECT_FALSE(
      tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
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

// Calls observers

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

TEST_F(TrackingProtectionSettingsTest,
       CorrectlyCallsObserversForFingerprintingProtection) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnFingerprintingProtectionEnabledChanged());
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnFingerprintingProtectionEnabledChanged());
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSettingsTest, CorrectlyCallsObserversForIpProtection) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnIpProtectionEnabledChanged());
  prefs()->SetBoolean(prefs::kIpProtectionEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnIpProtectionEnabledChanged());
  prefs()->SetBoolean(prefs::kIpProtectionEnabled, false);
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
