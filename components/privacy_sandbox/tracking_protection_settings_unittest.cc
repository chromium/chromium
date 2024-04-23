// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
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
  TrackingProtectionSettingsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    RegisterProfilePrefs(prefs()->registry());

    onboarding_service_ = std::make_unique<TrackingProtectionOnboarding>(
        &prefs_, version_info::Channel::UNKNOWN);
  }

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }

  void SetUp() override {
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false,
        /*should_record_metrics=*/false);
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kIpProtectionV1,
         privacy_sandbox::kFingerprintingProtectionSetting,
         privacy_sandbox::kTrackingProtectionSettingsLaunch},
        {});
    tracking_protection_settings_ =
        std::make_unique<TrackingProtectionSettings>(
            prefs(), host_content_settings_map_.get(),
            onboarding_service_.get(), /*is_incognito=*/false);
  }

  void TearDown() override {
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
  }

  TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_.get();
  }

  TrackingProtectionOnboarding* onboarding_service() {
    return onboarding_service_.get();
  }

  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TrackingProtectionOnboarding> onboarding_service_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<TrackingProtectionSettings> tracking_protection_settings_;
  base::test::SingleThreadTaskEnvironment task_environment_;
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
  EXPECT_TRUE(TrackingProtectionSettings(prefs(), host_content_settings_map(),
                                         nullptr,
                                         /*is_incognito=*/true)
                  .AreAllThirdPartyCookiesBlocked());
  EXPECT_FALSE(TrackingProtectionSettings(prefs(), host_content_settings_map(),
                                          nullptr,
                                          /*is_incognito=*/false)
                   .AreAllThirdPartyCookiesBlocked());
}

TEST_F(TrackingProtectionSettingsTest, AreAll3pcBlockedFalseOutside3pcd) {
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  EXPECT_FALSE(
      tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
}

TEST_F(TrackingProtectionSettingsTest,
       AreAll3pcBlockedFalseWhen3pcAllowedPrefTrue) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetBoolean(prefs::kAllowAll3pcToggleEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(
      tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
}

TEST_F(TrackingProtectionSettingsTest,
       Are3pcAllowedByEnterpriseTrueWhenPrefTrueIn3pcd) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_FALSE(tracking_protection_settings()
                   ->AreThirdPartyCookiesAllowedByEnterprise());
  EXPECT_CALL(observer, OnBlockAllThirdPartyCookiesChanged());
  prefs()->SetBoolean(prefs::kAllowAll3pcToggleEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(tracking_protection_settings()
                  ->AreThirdPartyCookiesAllowedByEnterprise());
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  EXPECT_FALSE(tracking_protection_settings()
                   ->AreThirdPartyCookiesAllowedByEnterprise());
}

TEST_F(TrackingProtectionSettingsTest,
       HasTrackingProtectionExceptionReturnsTrueForAllow) {
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GetTestUrl()),
      ContentSettingsType::TRACKING_PROTECTION,
      ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(tracking_protection_settings()->HasTrackingProtectionException(
      GetTestUrl()));
}

TEST_F(TrackingProtectionSettingsTest,
       HasTrackingProtectionExceptionReturnsFalseByDefault) {
  EXPECT_FALSE(tracking_protection_settings()->HasTrackingProtectionException(
      GetTestUrl()));
}

TEST_F(TrackingProtectionSettingsTest,
       AddTrackingProtectionExceptionAddsContentSetting) {
  tracking_protection_settings()->AddTrackingProtectionException(
      GetTestUrl(), /*is_user_bypass_exception=*/false);

  content_settings::SettingInfo info;
  EXPECT_EQ(host_content_settings_map()->GetContentSetting(
                GURL(), GetTestUrl(), ContentSettingsType::TRACKING_PROTECTION,
                &info),
            CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(info.metadata.expiration().is_null());
}

TEST_F(
    TrackingProtectionSettingsTest,
    AddTrackingProtectionExceptionAddsContentSettingWithUserBypassException) {
  tracking_protection_settings()->AddTrackingProtectionException(
      GetTestUrl(), /*is_user_bypass_exception=*/true);

  content_settings::SettingInfo info;
  EXPECT_EQ(host_content_settings_map()->GetContentSetting(
                GURL(), GetTestUrl(), ContentSettingsType::TRACKING_PROTECTION,
                &info),
            CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(info.metadata.expiration().is_null());
}

TEST_F(TrackingProtectionSettingsTest,
       RemoveTrackingProtectionExceptionRemovesContentSetting) {
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(GetTestUrl()),
      ContentSettingsType::TRACKING_PROTECTION,
      ContentSetting::CONTENT_SETTING_ALLOW);
  tracking_protection_settings()->RemoveTrackingProtectionException(
      GetTestUrl());

  EXPECT_EQ(host_content_settings_map()->GetContentSetting(
                GURL(), GetTestUrl(), ContentSettingsType::TRACKING_PROTECTION),
            CONTENT_SETTING_BLOCK);
}

TEST_F(TrackingProtectionSettingsTest,
       AddThenRemoveTrackingProtectionExceptionResetsContentSetting) {
  tracking_protection_settings()->AddTrackingProtectionException(
      GetTestUrl(), /*is_user_bypass_exception=*/false);

  content_settings::SettingInfo info;
  EXPECT_EQ(host_content_settings_map()->GetContentSetting(
                GURL(), GetTestUrl(), ContentSettingsType::TRACKING_PROTECTION,
                &info),
            CONTENT_SETTING_ALLOW);

  tracking_protection_settings()->RemoveTrackingProtectionException(
      GetTestUrl());

  EXPECT_EQ(host_content_settings_map()->GetContentSetting(
                GURL(), GetTestUrl(), ContentSettingsType::TRACKING_PROTECTION),
            CONTENT_SETTING_BLOCK);
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
