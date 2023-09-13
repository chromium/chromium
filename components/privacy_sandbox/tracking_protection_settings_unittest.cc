// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"
#include <memory>
#include <utility>
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

class MockTrackingProtectionSettingsObserver
    : public TrackingProtectionSettings::Observer {
 public:
  MOCK_METHOD(void, OnDoNotTrackEnabledChanged, (), (override));
  MOCK_METHOD(void, OnBlockAllThirdPartyCookiesChanged, (), (override));
};

class TrackingProtectionSettingsTest : public testing::Test {
 public:
  TrackingProtectionSettingsTest() {
    tracking_protection::RegisterProfilePrefs(prefs()->registry());
    onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(&prefs_);
  }

  void SetUp() override {
    tracking_protection_settings_ =
        std::make_unique<TrackingProtectionSettings>(prefs(),
                                                     onboarding_service_.get());
  }

  TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionOnboarding> onboarding_service_;
  std::unique_ptr<TrackingProtectionSettings> tracking_protection_settings_;
};

TEST_F(TrackingProtectionSettingsTest, ReturnsCustomTrackingProtectionLevel) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(tracking_protection::TrackingProtectionLevel::kCustom));
  EXPECT_EQ(tracking_protection_settings()->GetTrackingProtectionLevel(),
            tracking_protection::TrackingProtectionLevel::kCustom);
  EXPECT_TRUE(
      tracking_protection_settings()->IsCustomTrackingProtectionLevel());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsStandardTrackingProtectionLevel) {
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(
          tracking_protection::TrackingProtectionLevel::kStandard));
  EXPECT_EQ(tracking_protection_settings()->GetTrackingProtectionLevel(),
            tracking_protection::TrackingProtectionLevel::kStandard);
  EXPECT_TRUE(
      tracking_protection_settings()->IsStandardTrackingProtectionLevel());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsDoNotTrackStatus) {
  EXPECT_FALSE(tracking_protection_settings()->IsDoNotTrackEnabled());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, true);
  EXPECT_TRUE(tracking_protection_settings()->IsDoNotTrackEnabled());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsTrackingProtection3pcdStatus) {
  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_TRUE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
}

TEST_F(TrackingProtectionSettingsTest, ReturnsBlockAll3pcToggleStatus) {
  EXPECT_FALSE(
      tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(tracking_protection::TrackingProtectionLevel::kCustom));
  EXPECT_TRUE(tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
}

TEST_F(TrackingProtectionSettingsTest,
       SetsTrackingProtection3pcdStatusAfterOnboardingAndCallsObservers) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_FALSE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
  EXPECT_CALL(observer, OnDoNotTrackEnabledChanged());
  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());

  tracking_protection_settings()->OnTrackingProtectionOnboarded();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(
      tracking_protection_settings()->IsTrackingProtection3pcdEnabled());
}

TEST_F(TrackingProtectionSettingsTest, CorrectlyCallsObserversForDoNotTrack) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnDoNotTrackEnabledChanged());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnDoNotTrackEnabledChanged());
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(tracking_protection::TrackingProtectionLevel::kCustom));
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnDoNotTrackEnabledChanged());
  prefs()->SetBoolean(prefs::kEnableDoNotTrack, false);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnDoNotTrackEnabledChanged()).Times(0);
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(tracking_protection::TrackingProtectionLevel::kCustom));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSettingsTest, CorrectlyCallsObserversForBlockAll3pc) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(tracking_protection::TrackingProtectionLevel::kCustom));
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged()).Times(0);
  prefs()->SetInteger(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(tracking_protection::TrackingProtectionLevel::kCustom));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace
}  // namespace privacy_sandbox
