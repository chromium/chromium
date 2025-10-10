// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/permission_revocation_request.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char url1[] = "https://example1.com";
const char url2[] = "https://example2.com";
const char url3[] = "https://example3.com";

const ContentSettingsType notifications_type =
    ContentSettingsType::NOTIFICATIONS;
const ContentSettingsType revoked_notifications_type =
    ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS;

std::set<ContentSettingsType> abusive_permission_types(
    {ContentSettingsType::NOTIFICATIONS});

}  // namespace

class AbusiveNotificationPermissionsManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_database_manager_ = new MockSafeBrowsingDatabaseManager();
  }

  void TearDown() override { mock_database_manager_.reset(); }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_.get();
  }

  void AddAbusiveNotification(std::string url, ContentSetting cs) {
    content_settings::ContentSettingConstraints constraint;
    hcsm()->SetContentSettingDefaultScope(GURL(url), GURL(url),
                                          notifications_type, cs, constraint);
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(url), safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  }

  void AddRevokedAbusiveNotification(std::string url,
                                     ContentSetting cs,
                                     bool is_ignored) {
    AddAbusiveNotification(url, cs);
    content_settings::ContentSettingConstraints constraint;
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url), revoked_notifications_type,
        base::Value(base::Value::Dict().Set(
            safety_hub::kRevokedStatusDictKeyStr,
            is_ignored ? safety_hub::kIgnoreStr : safety_hub::kRevokeStr)),
        constraint);
  }

  void AddSafeNotification(std::string url, ContentSetting cs) {
    content_settings::ContentSettingConstraints constraint;
    hcsm()->SetContentSettingDefaultScope(GURL(url), GURL(url),
                                          notifications_type, cs, constraint);
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(url), safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE);
  }

  bool IsUrlInContentSettings(ContentSettingsForOneType content_settings,
                              std::string url) {
    std::string url_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url)).ToString();
    for (const auto& setting : content_settings) {
      if (setting.primary_pattern.ToString() == url_pattern) {
        return true;
      }
    }
    return false;
  }

  ContentSetting GetNotificationSettingValue(const std::string& url) {
    return hcsm()->GetContentSetting(GURL(url), GURL(url), notifications_type);
  }

  // TODO(crbug/com/342210522): When we refactor utils to be cleaner, this
  // helper will no longer be necessary.
  bool IsRevokedSettingValueRevoked(
      std::string url) {
    base::Value stored_value =
        safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
            hcsm(), GURL(url));
    if (stored_value.GetDict()
            .Find(safety_hub::kRevokedStatusDictKeyStr)
            ->GetString() == safety_hub::kRevokeStr) {
      return true;
    }
    return false;
  }

  void RunUntilSafeBrowsingChecksComplete(
      AbusiveNotificationPermissionsManager* manager) {
    manager->CheckNotificationPermissionOrigins();
    base::RunLoop().RunUntilIdle();
  }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
  }

  void VerifyTimeoutCallbackNotCalled() {
    // Verify timeout is not called even after fast forwarding.
    FastForwardBy(base::Milliseconds(kCheckUrlTimeoutMs));
    EXPECT_FALSE(mock_database_manager_->HasCalledCancelCheck());
  }

  TestingProfile* profile() { return &profile_; }

 private:
  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_manager_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
};

TEST_F(AbusiveNotificationPermissionsManagerTest,
       AddAllowedAbusiveNotificationSitesToRevokedOriginSet) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);
  base::HistogramTester histogram_tester;

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  VerifyTimeoutCallbackNotCalled();

  // Assert blocklist check count is recorded in UMA metrics.
  histogram_tester.ExpectUniqueSample(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 2,
      /* expected_count */ 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.NotificationRevocationSource",
      safe_browsing::NotificationRevocationSource::kSocialEngineeringBlocklist,
      /* expected_count */ 2);
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddSafeAbusiveNotificationSitesToRevokedOriginSet) {
  AddSafeNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);
  base::HistogramTester histogram_tester;

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm(), GURL(url1))
          .is_none());
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  VerifyTimeoutCallbackNotCalled();

  // Assert blocklist check count is recorded in UMA metrics.
  histogram_tester.ExpectUniqueSample(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 2,
      /* expected_count */ 1);
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddBlockedSettingToRevokedList) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ASK);
  AddRevokedAbusiveNotification(url3, ContentSetting::CONTENT_SETTING_ASK,
                                /*is_ignored=*/true);
  base::HistogramTester histogram_tester;

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url3),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  EXPECT_TRUE(
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm(), GURL(url2))
          .is_none());
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url3)));

  VerifyTimeoutCallbackNotCalled();

  // Assert blocklist check count is recorded in UMA metrics.
  histogram_tester.ExpectUniqueSample(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 1,
      /* expected_count */ 1);
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddIgnoredSettingToRevokedList) {
  AddRevokedAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ASK,
                                /*is_ignored=*/false);
  AddRevokedAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW,
                                /*is_ignored=*/true);
  AddRevokedAbusiveNotification(url3, ContentSetting::CONTENT_SETTING_ALLOW,
                                /*is_ignored=*/true);
  base::HistogramTester histogram_tester;

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetNotificationSettingValue(url3),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url2)));
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url3)));

  VerifyTimeoutCallbackNotCalled();

  // Assert blocklist check count is recorded in UMA metrics.
  histogram_tester.ExpectUniqueSample(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 0,
      /* expected_count */ 1);
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddAbusiveNotificationSitesOnTimeout) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  manager.SetNullSBCheckDelayForTesting();
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());
  RunUntilSafeBrowsingChecksComplete(&manager);
  EXPECT_TRUE(mock_database_manager()->HasCalledCancelCheck());
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       RegrantedPermissionShouldNotBeChecked) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  // Make sure that when we regrant url1, it is not automatically revoked again.
  manager.RegrantPermissionForOriginIfNecessary(GURL(url1));
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url1)));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  // Running period checks again should still not include url1.
  RunUntilSafeBrowsingChecksComplete(&manager);
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url1)));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  // Check that the correct metric is reported.
  auto ukm_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::
          SafetyHub_AbusiveNotificationPermissionRevocation_Interactions::
              kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());
  const auto* entry1 = ukm_entries[0].get();
  ukm_recorder.ExpectEntrySourceHasUrl(entry1, GURL(url1));
  ukm_recorder.ExpectEntryMetric(
      entry1, "InteractionType",
      static_cast<int>(
          AbusiveNotificationPermissionsManager::
              AbusiveNotificationPermissionsInteractions::kAllowAgain));
  ukm_recorder.ExpectEntryMetric(
      entry1, "RevocationSource",
      static_cast<int>(safe_browsing::NotificationRevocationSource::
                           kSocialEngineeringBlocklist));
}

TEST_F(AbusiveNotificationPermissionsManagerTest, ClearRevokedPermissionsList) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));

  manager.ClearRevokedPermissionsList();
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm(), GURL(url1))
          .is_none());
  EXPECT_TRUE(
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm(), GURL(url2))
          .is_none());
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       RestoreDeletedRevokedPermissionsList) {
  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);

  content_settings::ContentSettingConstraints constraints;
  manager.RestoreDeletedRevokedPermission(
      ContentSettingsPattern::FromURLNoWildcard(GURL(url1)),
      constraints.Clone());
  manager.RestoreDeletedRevokedPermission(
      ContentSettingsPattern::FromURLNoWildcard(GURL(url2)),
      constraints.Clone());

  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       SetRevokedAbusiveNotificationPermission) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));

  manager.ClearRevokedPermissionsList();
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);

  AbusiveNotificationPermissionsManager::
      SetRevokedAbusiveNotificationPermission(
          hcsm(), GURL(url1),
          /*is_ignored=*/false,
          safe_browsing::NotificationRevocationSource::
              kSocialEngineeringBlocklist);
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  EXPECT_TRUE(
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm(), GURL(url2))
          .is_none());

  AbusiveNotificationPermissionsManager::
      SetRevokedAbusiveNotificationPermission(
          hcsm(), GURL(url2),
          /*is_ignored=*/false,
          safe_browsing::NotificationRevocationSource::
              kSocialEngineeringBlocklist);
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));
  EXPECT_EQ(
      safe_browsing::NotificationRevocationSource::kSocialEngineeringBlocklist,
      AbusiveNotificationPermissionsManager::
          GetRevokedAbusiveNotificationRevocationSource(hcsm(), GURL(url1)));
  EXPECT_EQ(
      safe_browsing::NotificationRevocationSource::kSocialEngineeringBlocklist,
      AbusiveNotificationPermissionsManager::
          GetRevokedAbusiveNotificationRevocationSource(hcsm(), GURL(url2)));
}

TEST_F(
    AbusiveNotificationPermissionsManagerTest,
    SetRevokedAbusiveNotificationPermissionMaintainsExistingRevocationSource) {
  // Simulate existing revoked notification setting without revocation source.
  AddRevokedAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ASK,
                                /*is_ignored=*/false);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  // Simulate existing revoked notification setting with revocation source
  // `kManualSafeBrowsingRevocationStr`.
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);
  content_settings::ContentSettingConstraints constraint;
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url2), GURL(url2), revoked_notifications_type,
      base::Value(
          base::Value::Dict()
              .Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr)
              .Set(kAbusiveRevocationSourceKeyStr,
                   kManualSafeBrowsingRevocationStr)),
      constraint);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));
  EXPECT_EQ(
      safe_browsing::NotificationRevocationSource::
          kManualSafeBrowsingRevocation,
      AbusiveNotificationPermissionsManager::
          GetRevokedAbusiveNotificationRevocationSource(hcsm(), GURL(url2)));

  AbusiveNotificationPermissionsManager::
      SetRevokedAbusiveNotificationPermission(hcsm(), GURL(url1),
                                              /*is_ignored=*/true);
  AbusiveNotificationPermissionsManager::
      SetRevokedAbusiveNotificationPermission(hcsm(), GURL(url2),
                                              /*is_ignored=*/true);

  // Verify the setting values are changed to ignored.
  EXPECT_FALSE(IsRevokedSettingValueRevoked(url1));
  EXPECT_FALSE(IsRevokedSettingValueRevoked(url2));
  // Verify that the revocation sources are maintained.
  EXPECT_EQ(
      safe_browsing::NotificationRevocationSource::kUnknown,
      AbusiveNotificationPermissionsManager::
          GetRevokedAbusiveNotificationRevocationSource(hcsm(), GURL(url1)));
  EXPECT_EQ(
      safe_browsing::NotificationRevocationSource::
          kManualSafeBrowsingRevocation,
      AbusiveNotificationPermissionsManager::
          GetRevokedAbusiveNotificationRevocationSource(hcsm(), GURL(url2)));
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       UndoRegrantPermissionForOriginIfNecessary) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));

  // Make sure that when we regrant url1, it is not automatically revoked again.
  manager.RegrantPermissionForOriginIfNecessary(GURL(url1));
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url1)));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  content_settings::ContentSettingConstraints constraints;
  manager.UndoRegrantPermissionForOriginIfNecessary(
      GURL(url1), abusive_permission_types, std::move(constraints));
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  // Check that the correct metric is reported.
  auto ukm_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::
          SafetyHub_AbusiveNotificationPermissionRevocation_Interactions::
              kEntryName);
  ASSERT_EQ(2u, ukm_entries.size());
  const auto* entry1 = ukm_entries[0].get();
  ukm_recorder.ExpectEntrySourceHasUrl(entry1, GURL(url1));
  ukm_recorder.ExpectEntryMetric(
      entry1, "InteractionType",
      static_cast<int>(
          AbusiveNotificationPermissionsManager::
              AbusiveNotificationPermissionsInteractions::kAllowAgain));
  ukm_recorder.ExpectEntryMetric(
      entry1, "RevocationSource",
      static_cast<int>(safe_browsing::NotificationRevocationSource::
                           kSocialEngineeringBlocklist));
  const auto* entry2 = ukm_entries[1].get();
  ukm_recorder.ExpectEntrySourceHasUrl(entry2, GURL(url1));
  ukm_recorder.ExpectEntryMetric(
      entry2, "InteractionType",
      static_cast<int>(
          AbusiveNotificationPermissionsManager::
              AbusiveNotificationPermissionsInteractions::kUndoAllowAgain));
  ukm_recorder.ExpectEntryMetric(
      entry2, "RevocationSource",
      static_cast<int>(safe_browsing::NotificationRevocationSource::
                           kSocialEngineeringBlocklist));
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       RegrantAndUndoMaintainExistingRevocationSource) {
  // Simulate existing revoked notification setting with revocation source
  // `kManualSafeBrowsingRevocationStr`.
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ASK);
  content_settings::ContentSettingConstraints constraint;
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url1), GURL(url1), revoked_notifications_type,
      base::Value(
          base::Value::Dict()
              .Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr)
              .Set(kAbusiveRevocationSourceKeyStr,
                   kManualSafeBrowsingRevocationStr)),
      constraint);
  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());

  // Re-grant.
  manager.RegrantPermissionForOriginIfNecessary(GURL(url1));
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url1)));

  // Undo re-grant.
  content_settings::ContentSettingConstraints constraints;
  manager.UndoRegrantPermissionForOriginIfNecessary(
      GURL(url1), abusive_permission_types, std::move(constraints));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url1));

  ASSERT_EQ(
      safe_browsing::NotificationRevocationSource::
          kManualSafeBrowsingRevocation,
      AbusiveNotificationPermissionsManager::
          GetRevokedAbusiveNotificationRevocationSource(hcsm(), GURL(url1)));
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       CheckExpirationOfRevokedAbusiveNotifications) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  // Set up 2 urls with `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` settings,
  // then regrant one of them.
  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  manager.RegrantPermissionForOriginIfNecessary(GURL(url1));

  // The notifications should be allowed for `url1` and revoked for `url2`.
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url1)));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url2));

  // After 40 days, the `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` settings
  // should be cleaned up, `url1` should still have allowed notifications,
  // `url2` should still have revoked notifications, and the list of revoked
  // abusive notification permissions we show to the user should be empty.
  FastForwardBy(base::Days(40));
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm(), GURL(url1)));
  EXPECT_TRUE(
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm(), GURL(url2))
          .is_none());
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       CheckBlocklistRevocationsHappenOncePerDay) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);
  base::HistogramTester histogram_tester;

  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  RunUntilSafeBrowsingChecksComplete(&manager);
  ContentSettingsForOneType content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  // Assert blocklist check count is recorded in UMA metrics.
  histogram_tester.ExpectUniqueSample(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 2,
      /* expected_count */ 1);

  // Add another abusive origin. Running the checks again should not block the
  // abusive origin, because a check was just recently run.
  AddAbusiveNotification(url3, ContentSetting::CONTENT_SETTING_ALLOW);
  RunUntilSafeBrowsingChecksComplete(&manager);
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  histogram_tester.ExpectTotalCount(
      safety_hub::kBlocklistCheckCountHistogramName, /* expected_count */ 2);
  histogram_tester.ExpectBucketCount(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 2,
      /* expected_count */ 1);
  histogram_tester.ExpectBucketCount(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 0,
      /* expected_count */ 1);

  // Fast forwarding a day should revoke the new abusive origin.
  FastForwardBy(base::Days(1));
  RunUntilSafeBrowsingChecksComplete(&manager);
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 3u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url3));
  EXPECT_EQ(GetNotificationSettingValue(url3),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(url3));
  histogram_tester.ExpectTotalCount(
      safety_hub::kBlocklistCheckCountHistogramName, /* expected_count */ 3);
  histogram_tester.ExpectBucketCount(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 2,
      /* expected_count */ 1);
  histogram_tester.ExpectBucketCount(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 0,
      /* expected_count */ 1);
  histogram_tester.ExpectBucketCount(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 1,
      /* expected_count */ 1);
}

class ShowManualNotificationRevocationsTest
    : public AbusiveNotificationPermissionsManagerTest {
 public:
  ShowManualNotificationRevocationsTest() {
    feature_list_.InitAndEnableFeature(
        safe_browsing::kShowManualNotificationRevocationsSafetyHub);
  }

  void SetUp() override {
    AbusiveNotificationPermissionsManagerTest::SetUp();
    fake_database_manager_ =
        base::MakeRefCounted<CrowdDenyFakeSafeBrowsingDatabaseManager>();
    test_safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    test_safe_browsing_factory_->SetTestDatabaseManager(
        fake_database_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        test_safe_browsing_factory_->CreateSafeBrowsingService());
    safety_hub_test_util::CreateRevokedPermissionsService(profile());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    AbusiveNotificationPermissionsManagerTest::TearDown();
  }

  void AddToPreloadDataBlocklist(
      const GURL& origin,
      chrome_browser_crowd_deny::
          SiteReputation_NotificationUserExperienceQuality reputation_type,
      bool has_warning) {
    CrowdDenyPreloadData::SiteReputation reputation;
    reputation.set_notification_ux_quality(reputation_type);
    reputation.set_warning_only(has_warning);
    testing_preload_data_.SetOriginReputation(url::Origin::Create(origin),
                                              std::move(reputation));
  }

  void AddToSafeBrowsingBlocklist(const GURL& url) {
    safe_browsing::ThreatMetadata test_metadata;
    test_metadata.api_permissions.emplace("NOTIFICATIONS");
    fake_database_manager_->SetSimulatedMetadataForUrl(url, test_metadata);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  testing::ScopedCrowdDenyPreloadDataOverride testing_preload_data_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager>
      fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      test_safe_browsing_factory_;
};

TEST_F(ShowManualNotificationRevocationsTest,
       ManualRevocationAcklowedgeThenUndo) {
  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());
  base::HistogramTester histogram_tester;

  // Setup notification subscription.
  const GURL origin_to_revoke = GURL("https://origin.com/");
  hcsm()->SetContentSettingDefaultScope(origin_to_revoke, GURL(),
                                        ContentSettingsType::NOTIFICATIONS,
                                        CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));

  // Trigger crowd deny revocation.
  AddToSafeBrowsingBlocklist(origin_to_revoke);
  AddToPreloadDataBlocklist(
      origin_to_revoke, CrowdDenyPreloadData::SiteReputation::ABUSIVE_CONTENT,
      /*has_warning=*/false);
  base::MockOnceCallback<void(PermissionRevocationRequest::Outcome)>
      mock_callback_receiver;
  base::RunLoop run_loop;
  auto permission_revocation = std::make_unique<PermissionRevocationRequest>(
      profile(), origin_to_revoke, mock_callback_receiver.Get());
  EXPECT_CALL(mock_callback_receiver, Run(PermissionRevocationRequest::Outcome::
                                              PERMISSION_REVOKED_DUE_TO_ABUSE))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  // Check if the content setting turn to ASK, when auto-revocation happens.
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      1u);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));

  // Clearing the revoked permissions removes from the list shown to the user,
  // but keeps notifications permission revoked.
  manager.ClearRevokedPermissionsList();
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));

  // Undo clearing the revoked permission adds it back to the list shown to the
  // user and keeps notification permissions revoked.
  content_settings::ContentSettingConstraints constraints;
  manager.RestoreDeletedRevokedPermission(
      ContentSettingsPattern::FromURLNoWildcard(origin_to_revoke),
      constraints.Clone());
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      1u);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));

  // Assert notification auto-revocation is recorded in UMA metrics.
  EXPECT_EQ(
      1u, histogram_tester
              .GetAllSamples(
                  "Settings.SafetyHub.UnusedSitePermissionsModule.AutoRevoked2")
              .size());
  histogram_tester.ExpectBucketCount(
      "Settings.SafetyHub.UnusedSitePermissionsModule.AutoRevoked2",
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          ContentSettingsType::NOTIFICATIONS),
      1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.NotificationRevocationSource",
      safe_browsing::NotificationRevocationSource::
          kManualSafeBrowsingRevocation,
      /* expected_count */ 1);
}

TEST_F(ShowManualNotificationRevocationsTest,
       ManualRevocationRegrantPermission) {
  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());

  // Setup notification subscription.
  const GURL origin_to_revoke = GURL("https://origin.com/");
  hcsm()->SetContentSettingDefaultScope(origin_to_revoke, GURL(),
                                        ContentSettingsType::NOTIFICATIONS,
                                        CONTENT_SETTING_ALLOW);

  // Trigger crowd deny revocation.
  AddToSafeBrowsingBlocklist(origin_to_revoke);
  AddToPreloadDataBlocklist(
      origin_to_revoke, CrowdDenyPreloadData::SiteReputation::ABUSIVE_CONTENT,
      /*has_warning=*/false);
  base::MockOnceCallback<void(PermissionRevocationRequest::Outcome)>
      mock_callback_receiver;
  base::RunLoop run_loop;
  auto permission_revocation = std::make_unique<PermissionRevocationRequest>(
      profile(), origin_to_revoke, mock_callback_receiver.Get());
  EXPECT_CALL(mock_callback_receiver, Run(PermissionRevocationRequest::Outcome::
                                              PERMISSION_REVOKED_DUE_TO_ABUSE))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  // Regrant removes the permission from the list shown to the user and changes
  // the notification permission to allow.
  manager.RegrantPermissionForOriginIfNecessary(origin_to_revoke);
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));
  EXPECT_TRUE(
      PermissionRevocationRequest::IsOriginExemptedFromFutureRevocations(
          profile(), origin_to_revoke));

  // Trigger crowd deny again.
  base::RunLoop run_loop_new;
  auto permission_revocation_new =
      std::make_unique<PermissionRevocationRequest>(
          profile(), origin_to_revoke, mock_callback_receiver.Get());
  EXPECT_CALL(mock_callback_receiver,
              Run(PermissionRevocationRequest::Outcome::PERMISSION_NOT_REVOKED))
      .WillOnce(testing::InvokeWithoutArgs(
          [&run_loop_new]() { run_loop_new.Quit(); }));
  run_loop_new.Run();

  // Permission should not have been revoked, since the user allowed it from
  // Safety Hub.
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));
}

TEST_F(ShowManualNotificationRevocationsTest,
       ManualRevocationUndoRegrantPermission) {
  auto manager = AbusiveNotificationPermissionsManager(
      mock_database_manager(), hcsm(), profile()->GetTestingPrefService());

  // Setup notification subscription.
  const GURL origin_to_revoke = GURL("https://origin.com/");
  hcsm()->SetContentSettingDefaultScope(origin_to_revoke, GURL(),
                                        ContentSettingsType::NOTIFICATIONS,
                                        CONTENT_SETTING_ALLOW);

  // Trigger crowd deny revocation.
  AddToSafeBrowsingBlocklist(origin_to_revoke);
  AddToPreloadDataBlocklist(
      origin_to_revoke, CrowdDenyPreloadData::SiteReputation::ABUSIVE_CONTENT,
      /*has_warning=*/false);
  base::MockOnceCallback<void(PermissionRevocationRequest::Outcome)>
      mock_callback_receiver;
  base::RunLoop run_loop;
  auto permission_revocation = std::make_unique<PermissionRevocationRequest>(
      profile(), origin_to_revoke, mock_callback_receiver.Get());
  EXPECT_CALL(mock_callback_receiver, Run(PermissionRevocationRequest::Outcome::
                                              PERMISSION_REVOKED_DUE_TO_ABUSE))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  // Regrant removes the permission from the list shown to the user and changes
  // the notification permission to allow.
  manager.RegrantPermissionForOriginIfNecessary(origin_to_revoke);
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0u);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));

  // Undo regrant adds the permission back to the list and revokes the
  // permission again.
  content_settings::ContentSettingConstraints constraints;
  manager.UndoRegrantPermissionForOriginIfNecessary(
      origin_to_revoke, abusive_permission_types, std::move(constraints));
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      1u);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            hcsm()->GetContentSetting(origin_to_revoke, origin_to_revoke,
                                      ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(
      PermissionRevocationRequest::IsOriginExemptedFromFutureRevocations(
          profile(), origin_to_revoke));
}
