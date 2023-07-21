// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
