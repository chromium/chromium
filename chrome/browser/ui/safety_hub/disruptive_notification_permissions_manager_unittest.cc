// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/constants.h"
#include "content/public/browser/browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

class DisruptiveNotificationPermissionsManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<DisruptiveNotificationPermissionsManager>(
        hcsm(), site_engagement_service());
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  site_engagement::SiteEngagementService* site_engagement_service() {
    return site_engagement::SiteEngagementServiceFactory::GetForProfile(
        &profile_);
  }

  void SetNotificationPermission(GURL url, ContentSetting setting) {
    hcsm()->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, setting);
  }

  void SetDailyAverageNotificationCount(GURL url, int daily_average_count) {
    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(&profile_);
    notifications_engagement_service->RecordNotificationDisplayed(
        url, daily_average_count * 7);
  }

  int GetRevokedPermissionsCount() {
    return hcsm()
        ->GetSettingsForOneType(
            ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS)
        .size();
  }

  DisruptiveNotificationPermissionsManager* manager() { return manager_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;

  std::unique_ptr<DisruptiveNotificationPermissionsManager> manager_;
};

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       RevokeDisruptivePermission) {
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();

  base::Value stored_value(hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS));
  EXPECT_FALSE(stored_value.is_none());
}

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       DontRevokePermissionHighEngagement) {
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 90);

  manager()->RevokeDisruptiveNotifications();

  base::Value stored_value(hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS));
  EXPECT_TRUE(stored_value.is_none());
}

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       DontRevokePermissionLowNotificationCount) {
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 1);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();

  base::Value stored_value(hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS));
  EXPECT_TRUE(stored_value.is_none());
}

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       NotEligableNotificationContentSettings) {
  // Already blocked notification.
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_BLOCK);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);

  // Broad content setting.
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("[*.]example.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_ALLOW);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
}

TEST_F(DisruptiveNotificationPermissionsManagerTest, ManagedContentSetting) {
  content_settings::TestUtils::OverrideProvider(
      hcsm(), std::make_unique<content_settings::MockProvider>(),
      content_settings::ProviderType::kPolicyProvider);

  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
}

TEST_F(DisruptiveNotificationPermissionsManagerTest, DefaultBlock) {
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);

  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
}
