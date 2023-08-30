// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

void RecordNotification(permissions::NotificationsEngagementService* service,
                        GURL url,
                        int daily_average_count) {
  // This many notifications were recorded during the past week in total.
  int total_count = daily_average_count * 7;
  service->RecordNotificationDisplayed(url, total_count);
}

base::Time GetReferenceTime() {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2018 11:00:00 GMT", &time));
  return time;
}

}  // namespace

class NotificationPermissionReviewServiceTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(NotificationPermissionReviewServiceTest,
       IgnoreOriginForNotificationPermissionReview) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  std::string urls[] = {"https://google.com:443", "https://www.youtube.com:443",
                        "https://www.example.com:443"};
  map->SetContentSettingDefaultScope(GURL(urls[0]), GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(GURL(urls[1]), GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  auto notification_permissions = service->GetNotificationSiteListForReview();
  EXPECT_EQ(2UL, notification_permissions.size());

  // Add notification permission to block list and check if it will be not be
  // shown on the list.
  auto pattern_to_ignore = ContentSettingsPattern::FromString(urls[0]);
  service->AddPatternToNotificationPermissionReviewBlocklist(
      pattern_to_ignore, ContentSettingsPattern::Wildcard());
  notification_permissions = service->GetNotificationSiteListForReview();
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(notification_permissions[0].primary_pattern,
            ContentSettingsPattern::FromString(urls[1]));

  ContentSettingsForOneType ignored_patterns = map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  EXPECT_EQ(ignored_patterns.size(), 1UL);
  EXPECT_EQ(ignored_patterns[0].primary_pattern, pattern_to_ignore);

  // On blocking notifications for an unrelated site, nothing changes.
  map->SetContentSettingDefaultScope(GURL(urls[1]), GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_EQ(service->GetNotificationSiteListForReview().size(), 1UL);
  ignored_patterns = map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  EXPECT_EQ(ignored_patterns.size(), 1UL);
  EXPECT_EQ(ignored_patterns[0].primary_pattern, pattern_to_ignore);

  // If the permissions for an element of the block list are modified (i.e. no
  // longer ALLOWed), the element should be removed from the list.
  map->SetContentSettingDefaultScope(GURL(urls[0]), GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_BLOCK);
  ignored_patterns = map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  EXPECT_EQ(ignored_patterns.size(), 0UL);
  EXPECT_EQ(service->GetNotificationSiteListForReview().size(), 1UL);
  // The site is presented again if permissions are re-granted.
  map->SetContentSettingDefaultScope(GURL(urls[0]), GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_EQ(service->GetNotificationSiteListForReview().size(), 2UL);
}

// TODO(crbug.com/1363714): Move this test to ContentSettingsPatternTest.
TEST_F(NotificationPermissionReviewServiceTest, SingleOriginTest) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  auto pattern_1 =
      ContentSettingsPattern::FromString("https://[*.]example1.com:443");
  auto pattern_2 =
      ContentSettingsPattern::FromString("https://example2.com:443");
  map->SetContentSettingCustomScope(
      pattern_1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      pattern_2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  // Assert wildcard in primary pattern returns false on single origin check.
  EXPECT_EQ(false, content_settings::PatternAppliesToSingleOrigin(
                       pattern_1, ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(true, content_settings::PatternAppliesToSingleOrigin(
                      pattern_2, ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(false, content_settings::PatternAppliesToSingleOrigin(pattern_1,
                                                                  pattern_2));

  // Assert the review list only has the URL with single origin.
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  auto notification_permissions = service->GetNotificationSiteListForReview();
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(pattern_2, notification_permissions[0].primary_pattern);
}

TEST_F(NotificationPermissionReviewServiceTest,
       ShowOnlyGrantedNotificationPermissions) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  GURL urls[] = {GURL("https://google.com/"), GURL("https://www.youtube.com/"),
                 GURL("https://www.example.com/")};
  map->SetContentSettingDefaultScope(urls[0], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(urls[1], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(
      urls[2], GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ASK);

  // Assert the review list only has the URL with granted permission.
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  auto notification_permissions = service->GetNotificationSiteListForReview();
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(GURL(notification_permissions[0].primary_pattern.ToString()),
            urls[0]);
}

TEST_F(NotificationPermissionReviewServiceTest,
       PopulateNotificationPermissionReviewData) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      features::kSafetyCheckNotificationPermissions);

  // Add a couple of notification permission and check they appear in review
  // list.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  GURL urls[] = {GURL("https://google.com:443"),
                 GURL("https://www.youtube.com:443"),
                 GURL("https://www.example.com:443")};

  map->SetContentSettingDefaultScope(urls[0], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(urls[1], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(urls[2], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);

  // Record initial display date to enable comparing dictionaries for
  // NotificationEngagementService.
  auto* notification_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(profile());
  std::string displayedDate =
      notification_engagement_service->GetBucketLabel(base::Time::Now());

  auto* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());

  // Set a host to have minimum engagement. This should be in review list.
  RecordNotification(notification_engagement_service, urls[0], 1);
  site_engagement::SiteEngagementScore score =
      site_engagement_service->CreateEngagementScore(urls[0]);
  score.Reset(0.5, GetReferenceTime());
  score.Commit();
  EXPECT_EQ(blink::mojom::EngagementLevel::MINIMAL,
            site_engagement_service->GetEngagementLevel(urls[0]));

  // Set a host to have large number of notifications, but low engagement. This
  // should be in review list.
  RecordNotification(notification_engagement_service, urls[1], 5);
  site_engagement_service->AddPointsForTesting(urls[1], 1.0);
  EXPECT_EQ(blink::mojom::EngagementLevel::LOW,
            site_engagement_service->GetEngagementLevel(urls[1]));

  // Set a host to have medium engagement and high notification count. This
  // should not be in review list.
  RecordNotification(notification_engagement_service, urls[2], 5);
  site_engagement_service->AddPointsForTesting(urls[2], 50.0);
  EXPECT_EQ(blink::mojom::EngagementLevel::MEDIUM,
            site_engagement_service->GetEngagementLevel(urls[2]));

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());

  const auto& notification_permissions =
      service->PopulateNotificationPermissionReviewData(profile());
  // Check if resulting list contains only the expected URLs.
  EXPECT_EQ(2UL, notification_permissions.size());
  EXPECT_EQ("https://www.youtube.com:443",
            *notification_permissions[0].GetDict().FindString(
                site_settings::kOrigin));
  EXPECT_EQ("About 5 notifications a day",
            *notification_permissions[0].GetDict().FindString(
                kSafetyHubNotificationInfoString));
  EXPECT_EQ("https://google.com:443",
            *notification_permissions[1].GetDict().FindString(
                site_settings::kOrigin));
  EXPECT_EQ("About 1 notification a day",
            *notification_permissions[1].GetDict().FindString(
                kSafetyHubNotificationInfoString));

  // Increasing notification count also promotes host in the list.
  RecordNotification(notification_engagement_service,
                     GURL("https://google.com:443"), 10);
  const auto& updated_notification_permissions =
      service->PopulateNotificationPermissionReviewData(profile());
  EXPECT_EQ(2UL, updated_notification_permissions.size());
  EXPECT_EQ("https://google.com:443",
            *updated_notification_permissions[0].GetDict().FindString(
                site_settings::kOrigin));
  EXPECT_EQ("About 11 notifications a day",
            *updated_notification_permissions[0].GetDict().FindString(
                kSafetyHubNotificationInfoString));
  EXPECT_EQ("https://www.youtube.com:443",
            *updated_notification_permissions[1].GetDict().FindString(
                site_settings::kOrigin));
  EXPECT_EQ("About 5 notifications a day",
            *updated_notification_permissions[1].GetDict().FindString(
                kSafetyHubNotificationInfoString));
}
