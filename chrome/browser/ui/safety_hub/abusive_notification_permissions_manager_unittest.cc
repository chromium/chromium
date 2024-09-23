// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "content/public/browser/browser_context.h"
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
      AbusiveNotificationPermissionsManager* abuse_manager,
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

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));

  VerifyTimeoutCallbackNotCalled();

  // Assert blocklist check count is recorded in UMA metrics.
  histogram_tester.ExpectUniqueSample(
      safety_hub::kBlocklistCheckCountHistogramName, /* sample */ 2,
      /* expected_count */ 1);
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddSafeAbusiveNotificationSitesToRevokedOriginSet) {
  AddSafeNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);
  base::HistogramTester histogram_tester;

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));

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

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url1));
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

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url1));
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

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));

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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));

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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));
}

TEST_F(AbusiveNotificationPermissionsManagerTest, ClearRevokedPermissionsList) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
       SetRevokedAbusiveNotificationPermission) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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

  safety_hub_util::SetRevokedAbusiveNotificationPermission(
      hcsm(), GURL(url1), /*is_ignored=*/false);
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 1u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url1));
  EXPECT_TRUE(
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm(), GURL(url2))
          .is_none());

  safety_hub_util::SetRevokedAbusiveNotificationPermission(
      hcsm(), GURL(url2), /*is_ignored=*/false);
  content_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  EXPECT_EQ(content_settings.size(), 2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       UndoRegrantPermissionForOriginIfNecessary) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));

  content_settings::ContentSettingConstraints constraints;
  manager.UndoRegrantPermissionForOriginIfNecessary(
      GURL(url1), abusive_permission_types, constraints);
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      2u);
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url1));
  EXPECT_TRUE(IsUrlInContentSettings(content_settings, url2));
  EXPECT_EQ(GetNotificationSettingValue(url1),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(GetNotificationSettingValue(url2),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url1));
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       CheckExpirationOfRevokedAbusiveNotifications) {
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW);

  // Set up 2 urls with `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` settings,
  // then regrant one of them.
  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
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
  EXPECT_TRUE(IsRevokedSettingValueRevoked(&manager, url2));

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
