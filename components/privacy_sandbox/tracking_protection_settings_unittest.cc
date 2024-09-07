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
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
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
  }

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }

  void SetUp() override {
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false,
        /*should_record_metrics=*/false);
    feature_list_.InitWithFeatures({privacy_sandbox::kIpProtectionV1}, {});
    tracking_protection_settings_ =
        std::make_unique<TrackingProtectionSettings>(
            prefs(), host_content_settings_map_.get(),
            /*is_incognito=*/false);
  }

  void TearDown() override {
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
  }

  TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_.get();
  }

  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::test::ScopedFeatureList feature_list_;
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
                                         /*is_incognito=*/true)
                  .AreAllThirdPartyCookiesBlocked());
  EXPECT_FALSE(TrackingProtectionSettings(prefs(), host_content_settings_map(),
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
       GetTrackingProtectionSettingReturnsAllow) {
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GetTestUrl()),
      ContentSettingsType::TRACKING_PROTECTION,
      ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(tracking_protection_settings()->GetTrackingProtectionSetting(
                GetTestUrl()),
            CONTENT_SETTING_ALLOW);
}

TEST_F(TrackingProtectionSettingsTest,
       GetTrackingProtectionSettingReturnsBlockByDefault) {
  EXPECT_EQ(tracking_protection_settings()->GetTrackingProtectionSetting(
                GetTestUrl()),
            CONTENT_SETTING_BLOCK);
}

TEST_F(TrackingProtectionSettingsTest,
       GetTrackingProtectionSettingFillsSettingInfo) {
  content_settings::TestUtils::OverrideProvider(
      host_content_settings_map(),
      std::make_unique<content_settings::MockProvider>(),
      content_settings::ProviderType::kPolicyProvider);
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GetTestUrl()),
      ContentSettingsType::TRACKING_PROTECTION,
      ContentSetting::CONTENT_SETTING_ALLOW);

  content_settings::SettingInfo info;
  EXPECT_EQ(tracking_protection_settings()->GetTrackingProtectionSetting(
                GetTestUrl(), &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(info.primary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(info.secondary_pattern,
            ContentSettingsPattern::FromURL(GetTestUrl()));
  EXPECT_EQ(info.source, content_settings::SettingSource::kPolicy);
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
      ContentSettingsPattern::Wildcard(),
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

}  // namespace
}  // namespace privacy_sandbox
