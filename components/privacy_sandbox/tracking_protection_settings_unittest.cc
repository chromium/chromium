// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
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
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

MATCHER_P(IsSameSite, site, "") {
  return net::SchemefulSite::IsSameSite(site, arg);
}

class MockTrackingProtectionSettingsObserver
    : public TrackingProtectionSettingsObserver {
 public:
  MOCK_METHOD(void, OnDoNotTrackEnabledChanged, (), (override));
  MOCK_METHOD(void, OnIpProtectionEnabledChanged, (), (override));
  MOCK_METHOD(void, OnFpProtectionEnabledChanged, (), (override));
  MOCK_METHOD(void, OnBlockAllThirdPartyCookiesChanged, (), (override));
  MOCK_METHOD(void, OnTrackingProtection3pcdChanged, (), (override));
  MOCK_METHOD(void,
              OnTrackingProtectionExceptionsChanged,
              (const GURL&),
              (override));
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

  virtual std::vector<base::test::FeatureRef> EnabledFeatures() {
    return {privacy_sandbox::kIpProtectionUx,
            privacy_sandbox::kFingerprintingProtectionUx};
  }

  void SetUp() override {
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false,
        /*should_record_metrics=*/false);
    feature_list_.InitWithFeatures(EnabledFeatures(), {});
    management_service_ = std::make_unique<policy::ManagementService>(
        std::vector<std::unique_ptr<policy::ManagementStatusProvider>>());
    tracking_protection_settings_ =
        std::make_unique<TrackingProtectionSettings>(
            prefs(), host_content_settings_map_.get(),
            management_service_.get(),
            /*is_incognito=*/false);
  }

  void TearDown() override {
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
    feature_list_.Reset();
  }

  TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_.get();
  }

  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }

  policy::ManagementService* management_service() {
    return management_service_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<policy::ManagementService> management_service_;
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
  prefs()->SetBoolean(prefs::kIpProtectionEnabled, false);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kIpProtectionEnabled));
  EXPECT_FALSE(tracking_protection_settings()->IsIpProtectionEnabled());
  prefs()->SetBoolean(prefs::kIpProtectionEnabled, true);
  EXPECT_TRUE(tracking_protection_settings()->IsIpProtectionEnabled());
}

TEST_F(TrackingProtectionSettingsTest,
       IsFpProtectionEnabledOnlyReturnsTrueInIncognito) {
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled, true);
  EXPECT_TRUE(TrackingProtectionSettings(prefs(), host_content_settings_map(),
                                         management_service(),
                                         /*is_incognito=*/true)
                  .IsFpProtectionEnabled());
  EXPECT_FALSE(TrackingProtectionSettings(prefs(), host_content_settings_map(),
                                          management_service(),
                                          /*is_incognito=*/false)
                   .IsFpProtectionEnabled());
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
                                         management_service(),
                                         /*is_incognito=*/true)
                  .AreAllThirdPartyCookiesBlocked());
  EXPECT_FALSE(TrackingProtectionSettings(prefs(), host_content_settings_map(),
                                          management_service(),
                                          /*is_incognito=*/false)
                   .AreAllThirdPartyCookiesBlocked());
}

TEST_F(TrackingProtectionSettingsTest, AreAll3pcBlockedFalseOutside3pcd) {
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  EXPECT_FALSE(
      tracking_protection_settings()->AreAllThirdPartyCookiesBlocked());
}

// Content settings

using HasTrackingProtectionExceptionTest = TrackingProtectionSettingsTest;

TEST_F(HasTrackingProtectionExceptionTest,
       ReturnsTrueWhenTrackingProtectionContentSettingForUrlIsAllow) {
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GetTestUrl()),
      ContentSettingsType::TRACKING_PROTECTION,
      ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(tracking_protection_settings()->HasTrackingProtectionException(
      GetTestUrl()));
}

TEST_F(HasTrackingProtectionExceptionTest, ReturnsFalseByDefault) {
  EXPECT_FALSE(tracking_protection_settings()->HasTrackingProtectionException(
      GetTestUrl()));
}

TEST_F(HasTrackingProtectionExceptionTest, FillsSettingInfo) {
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
  EXPECT_TRUE(tracking_protection_settings()->HasTrackingProtectionException(
      GetTestUrl(), &info));
  EXPECT_EQ(info.primary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(info.secondary_pattern,
            ContentSettingsPattern::FromURL(GetTestUrl()));
  EXPECT_EQ(info.source, content_settings::SettingSource::kPolicy);
}

TEST_F(TrackingProtectionSettingsTest,
       AddTrackingProtectionExceptionAddsContentSetting) {
  tracking_protection_settings()->AddTrackingProtectionException(GetTestUrl());
  content_settings::SettingInfo info;
  EXPECT_EQ(host_content_settings_map()->GetContentSetting(
                GURL(), GetTestUrl(), ContentSettingsType::TRACKING_PROTECTION,
                &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.metadata.expiration().is_null());
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

// Tests that `GetTrackingProtectionExceptions` correctly filters its results.
// The method should only return content settings with a value of ALLOW, as
// these represent exceptions. It should not return settings of type
// TRACKING_PROTECTION with other values, such as BLOCK.
TEST_F(TrackingProtectionSettingsTest,
       GetTrackingProtectionExceptionsReturnsOnlyAllowed) {
  // Add a user-created exception, which is stored as a content setting with a
  // value of ALLOW.
  tracking_protection_settings()->AddTrackingProtectionException(GetTestUrl());
  // In addition, manually add a content setting for the same feature but with a
  // value of BLOCK. This simulates other potential rules that are not user
  // exceptions.
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(
          GURL("http://another.url.com")),
      ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_BLOCK);

  // Verify that the method correctly filters the results and returns only the
  // ALLOW setting.
  ContentSettingsForOneType exceptions =
      tracking_protection_settings()->GetTrackingProtectionExceptions();
  ASSERT_EQ(exceptions.size(), 1u);
  EXPECT_EQ(exceptions[0].GetContentSetting(), CONTENT_SETTING_ALLOW);
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

TEST_F(TrackingProtectionSettingsTest, CorrectlyCallsObserversForFpp) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnFpProtectionEnabledChanged());
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnFpProtectionEnabledChanged());
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled, false);
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

TEST_F(TrackingProtectionSettingsTest,
       CorrectlyCallsObserversForTrackingProtectionExceptions) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer,
              OnTrackingProtectionExceptionsChanged(IsSameSite(GetTestUrl())));
  tracking_protection_settings()->AddTrackingProtectionException(GetTestUrl());
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnTrackingProtectionExceptionsChanged(IsSameSite(GetTestUrl())));
  tracking_protection_settings()->RemoveTrackingProtectionException(
      GetTestUrl());
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSettingsTest,
       CorrectlyCallsObserversForDirectContentSettingChanges) {
  MockTrackingProtectionSettingsObserver observer;
  tracking_protection_settings()->AddObserver(&observer);

  EXPECT_CALL(observer,
              OnTrackingProtectionExceptionsChanged(IsSameSite(GetTestUrl())));
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(GetTestUrl()),
      ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

// Rollback does not apply to iOS.
#if !BUILDFLAG(IS_IOS)

class MaybeSetRollbackPrefsModeBTest : public TrackingProtectionSettingsTest {
 public:
  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {privacy_sandbox::kRollBackModeB};
  }

  void Initialize3pcdState(content_settings::CookieControlsMode cookies_mode,
                           bool all_3pcs_blocked) {
    prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
    prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, all_3pcs_blocked);
    prefs()->SetInteger(prefs::kCookieControlsMode,
                        static_cast<int>(cookies_mode));
  }

  void VerifyRollbackState(content_settings::CookieControlsMode cookies_mode,
                           bool show_rollback_ui) {
    EXPECT_FALSE(prefs()->GetBoolean(prefs::kTrackingProtection3pcdEnabled));
    EXPECT_EQ(prefs()->GetBoolean(prefs::kShowRollbackUiModeB),
              show_rollback_ui);
    EXPECT_EQ(prefs()->GetInteger(prefs::kCookieControlsMode),
              static_cast<int>(cookies_mode));
    histogram_tester_.ExpectUniqueSample(
        "Privacy.3PCD.RollbackNotice.ShouldShow", show_rollback_ui, 1);
  }

  void SetSyncStatus(syncer::SyncService::DataTypeDownloadStatus status) {
    test_sync_service_.SetDownloadStatusFor({syncer::DataType::PREFERENCES},
                                            status);
  }

  syncer::TestSyncService* test_sync_service() { return &test_sync_service_; }

 private:
  syncer::TestSyncService test_sync_service_;
  base::HistogramTester histogram_tester_;
};

TEST_F(MaybeSetRollbackPrefsModeBTest, ShowsNoticeWhen3pcsAllowed) {
  SetSyncStatus(syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  Initialize3pcdState(content_settings::CookieControlsMode::kOff, false);
  MaybeSetRollbackPrefsModeB(test_sync_service(), prefs());
  VerifyRollbackState(content_settings::CookieControlsMode::kOff, true);
}

TEST_F(MaybeSetRollbackPrefsModeBTest, DoesNotOffboardWhenWaitingForPrefSync) {
  SetSyncStatus(
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  Initialize3pcdState(content_settings::CookieControlsMode::kOff, false);
  MaybeSetRollbackPrefsModeB(test_sync_service(), prefs());
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kTrackingProtection3pcdEnabled));
}

TEST_F(MaybeSetRollbackPrefsModeBTest,
       Blocks3pcsAndDoesNotShowNoticeWhen3pcsBlockedIn3pcd) {
  SetSyncStatus(syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  Initialize3pcdState(content_settings::CookieControlsMode::kOff, true);
  MaybeSetRollbackPrefsModeB(test_sync_service(), prefs());
  VerifyRollbackState(content_settings::CookieControlsMode::kBlockThirdParty,
                      false);
}

TEST_F(MaybeSetRollbackPrefsModeBTest, DoesNotShowNoticeWhen3pcsBlocked) {
  SetSyncStatus(syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  Initialize3pcdState(content_settings::CookieControlsMode::kBlockThirdParty,
                      false);
  MaybeSetRollbackPrefsModeB(test_sync_service(), prefs());
  VerifyRollbackState(content_settings::CookieControlsMode::kBlockThirdParty,
                      false);
}

#endif

}  // namespace
}  // namespace privacy_sandbox
