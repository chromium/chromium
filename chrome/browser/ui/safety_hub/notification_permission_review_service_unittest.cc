// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "content/public/test/browser_task_environment.h"
#include "notification_permission_review_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class NotificationPermissionReviewServiceTest : public testing::Test {
 protected:
  void CreateMockNotificationPermissionsForReview() {
    // Add a couple of notification permission and check they appear in review
    // list.
    GURL urls[] = {GURL("https://google.com:443"),
                   GURL("https://www.youtube.com:443"),
                   GURL("https://www.example.com:443")};

    auto* site_engagement_service =
        site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());

    // Set a host to have minimum engagement. This should be in review list.
    SetNotificationPermissionAndRecordEngagement(urls[0], CONTENT_SETTING_ALLOW,
                                                 1);
    site_engagement::SiteEngagementScore score =
        site_engagement_service->CreateEngagementScore(urls[0]);
    score.Reset(0.5, base::Time::Now());
    score.Commit();
    EXPECT_EQ(blink::mojom::EngagementLevel::MINIMAL,
              site_engagement_service->GetEngagementLevel(urls[0]));

    // Set a host to have large number of notifications, but low engagement.
    // This should be in review list.
    SetNotificationPermissionAndRecordEngagement(urls[1], CONTENT_SETTING_ALLOW,
                                                 5);
    site_engagement_service->AddPointsForTesting(urls[1], 1.0);
    EXPECT_EQ(blink::mojom::EngagementLevel::LOW,
              site_engagement_service->GetEngagementLevel(urls[1]));

    // Set a host to have medium engagement and high notification count. This
    // should not be in review list.
    SetNotificationPermissionAndRecordEngagement(urls[2], CONTENT_SETTING_ALLOW,
                                                 5);
    site_engagement_service->AddPointsForTesting(urls[2], 50.0);
    EXPECT_EQ(blink::mojom::EngagementLevel::MEDIUM,
              site_engagement_service->GetEngagementLevel(urls[2]));
  }

  std::vector<NotificationPermissions> GetUpdatedReviewList(
      NotificationPermissionsReviewService* service) {
    safety_hub_test_util::UpdateSafetyHubServiceAsync(service);
    std::optional<std::unique_ptr<SafetyHubService::Result>> result_opt =
        service->GetCachedResult();
    EXPECT_TRUE(result_opt.has_value());
    auto* result = static_cast<
        NotificationPermissionsReviewService::NotificationPermissionsResult*>(
        result_opt.value().get());
    return result->GetSortedNotificationPermissions();
  }

  void SetNotificationPermissionAndRecordEngagement(GURL url,
                                                    ContentSetting setting,
                                                    int daily_average_count,
                                                    GURL secondary = GURL()) {
    hcsm()->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, setting);
    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(profile());
    notifications_engagement_service->RecordNotificationDisplayed(
        url, daily_average_count * 7);
  }

  const std::vector<NotificationPermissions>
  GetNotificationPermissionsFromService() {
    auto* service =
        NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
    std::optional<std::unique_ptr<SafetyHubService::Result>> sh_result =
        service->GetCachedResult();
    EXPECT_TRUE(sh_result.has_value());
    return static_cast<NotificationPermissionsReviewService::
                           NotificationPermissionsResult*>(
               std::move(sh_result)->get())
        ->GetSortedNotificationPermissions();
  }

  TestingProfile* profile() { return &profile_; }
  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(NotificationPermissionReviewServiceTest,
       IgnoreOriginForNotificationPermissionReview) {
  std::string urls[] = {"https://google.com:443", "https://www.youtube.com:443",
                        "https://www.example.com:443"};
  SetNotificationPermissionAndRecordEngagement(GURL(urls[0]),
                                               CONTENT_SETTING_ALLOW, 1);
  SetNotificationPermissionAndRecordEngagement(GURL(urls[1]),
                                               CONTENT_SETTING_ALLOW, 1);
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  EXPECT_EQ(2UL, GetUpdatedReviewList(service).size());

  // Add notification permission to block list and check if it will be not be
  // shown on the list.
  auto pattern_to_ignore = ContentSettingsPattern::FromString(urls[0]);
  service->AddPatternToNotificationPermissionReviewBlocklist(
      pattern_to_ignore, ContentSettingsPattern::Wildcard());

  std::vector<NotificationPermissions> notification_permissions =
      GetUpdatedReviewList(service);
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(notification_permissions[0].primary_pattern,
            ContentSettingsPattern::FromString(urls[1]));

  ContentSettingsForOneType ignored_patterns = hcsm()->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  EXPECT_EQ(ignored_patterns.size(), 1UL);
  EXPECT_EQ(ignored_patterns[0].primary_pattern, pattern_to_ignore);

  // On blocking notifications for an unrelated site, nothing changes.
  hcsm()->SetContentSettingDefaultScope(GURL(urls[2]), GURL(),
                                        ContentSettingsType::NOTIFICATIONS,
                                        CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetUpdatedReviewList(service).size(), 1UL);
  ignored_patterns = hcsm()->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  EXPECT_EQ(ignored_patterns.size(), 1UL);
  EXPECT_EQ(ignored_patterns[0].primary_pattern, pattern_to_ignore);

  // If the permissions for an element of the block list are modified (i.e. no
  // longer ALLOWed), the element should be removed from the list.
  hcsm()->SetContentSettingDefaultScope(GURL(urls[0]), GURL(),
                                        ContentSettingsType::NOTIFICATIONS,
                                        CONTENT_SETTING_BLOCK);
  ignored_patterns = hcsm()->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  EXPECT_EQ(ignored_patterns.size(), 0UL);
  EXPECT_EQ(GetUpdatedReviewList(service).size(), 1UL);
  // The site is presented again if permissions are re-granted.
  SetNotificationPermissionAndRecordEngagement(GURL(urls[0]),
                                               CONTENT_SETTING_ALLOW, 1);
  EXPECT_EQ(GetUpdatedReviewList(service).size(), 2UL);
}

// TODO(crbug.com/40865125): Move this test to ContentSettingsPatternTest.
TEST_F(NotificationPermissionReviewServiceTest, SingleOriginTest) {
  auto pattern_1 =
      ContentSettingsPattern::FromString("https://[*.]example1.com:443");
  auto pattern_2 =
      ContentSettingsPattern::FromString("https://example2.com:443");
  hcsm()->SetContentSettingCustomScope(
      pattern_1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  hcsm()->SetContentSettingCustomScope(
      pattern_2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  auto* notifications_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(profile());
  notifications_engagement_service->RecordNotificationDisplayed(
      pattern_1.ToRepresentativeUrl(), 7);
  notifications_engagement_service->RecordNotificationDisplayed(
      pattern_2.ToRepresentativeUrl(), 7);

  // Assert wildcard in primary pattern returns false on single origin check.
  EXPECT_FALSE(content_settings::PatternAppliesToSingleOrigin(
      pattern_1, ContentSettingsPattern::Wildcard()));
  EXPECT_TRUE(content_settings::PatternAppliesToSingleOrigin(
      pattern_2, ContentSettingsPattern::Wildcard()));
  EXPECT_FALSE(
      content_settings::PatternAppliesToSingleOrigin(pattern_1, pattern_2));

  // Assert the review list only has the URL with single origin.
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  std::vector<NotificationPermissions> notification_permissions =
      GetUpdatedReviewList(service);
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(pattern_2, notification_permissions[0].primary_pattern);
}

TEST_F(NotificationPermissionReviewServiceTest,
       ShowOnlyGrantedNotificationPermissions) {
  GURL urls[] = {GURL("https://google.com/"), GURL("https://www.youtube.com/"),
                 GURL("https://www.example.com/")};
  SetNotificationPermissionAndRecordEngagement(urls[0], CONTENT_SETTING_ALLOW,
                                               1);
  SetNotificationPermissionAndRecordEngagement(urls[1], CONTENT_SETTING_BLOCK,
                                               1);
  SetNotificationPermissionAndRecordEngagement(urls[2], CONTENT_SETTING_ASK, 1);

  // Assert the review list only has the URL with granted permission.
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  std::vector<NotificationPermissions> notification_permissions =
      GetUpdatedReviewList(service);
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(GURL(notification_permissions[0].primary_pattern.ToString()),
            urls[0]);
}

TEST_F(NotificationPermissionReviewServiceTest,
       PopulateNotificationPermissionReviewData) {
  CreateMockNotificationPermissionsForReview();

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());

  const auto& notification_permissions =
      service->PopulateNotificationPermissionReviewData();
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
  auto* notification_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(profile());
  notification_engagement_service->RecordNotificationDisplayed(
      GURL("https://google.com:443"), 10 * 7);
  // Letting service update the cached result.
  safety_hub_test_util::UpdateSafetyHubServiceAsync(service);
  const auto& updated_notification_permissions =
      service->PopulateNotificationPermissionReviewData();
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

TEST_F(NotificationPermissionReviewServiceTest, ResultToDict) {
  auto origin = ContentSettingsPattern::FromString("https://example1.com:443");
  const int notification_count = 1337;

  auto result = std::make_unique<
      NotificationPermissionsReviewService::NotificationPermissionsResult>();
  result->AddNotificationPermission(NotificationPermissions(
      origin, ContentSettingsPattern::Wildcard(), notification_count));
  EXPECT_THAT(result->GetOrigins(), testing::ElementsAre(origin));

  // When converting to dict, the values of the notification permissions should
  // be correctly converted to base::Value.
  base::Value::Dict dict = result->ToDictValue();
  auto* notification_perms_list =
      dict.FindList(kSafetyHubNotificationPermissionsResultKey);
  EXPECT_EQ(1U, notification_perms_list->size());

  base::Value::Dict& notification_perm =
      notification_perms_list->front().GetDict();
  EXPECT_EQ(origin.ToString(),
            *notification_perm.FindString(kSafetyHubOriginKey));
}

TEST_F(NotificationPermissionReviewServiceTest, ResultGetOrigins) {
  auto origin1 = ContentSettingsPattern::FromString("https://example1.com:443");
  auto origin2 = ContentSettingsPattern::FromString("https://example2.com:443");
  auto result = std::make_unique<
      NotificationPermissionsReviewService::NotificationPermissionsResult>();
  EXPECT_EQ(0U, result->GetOrigins().size());
  result->AddNotificationPermission(
      NotificationPermissions(origin1, ContentSettingsPattern::Wildcard(), 42));
  EXPECT_EQ(1U, result->GetOrigins().size());
  EXPECT_EQ(origin1, *result->GetOrigins().begin());
  result->AddNotificationPermission(NotificationPermissions(
      origin2, ContentSettingsPattern::Wildcard(), 123));
  EXPECT_EQ(2U, result->GetOrigins().size());
  EXPECT_TRUE(result->GetOrigins().contains(origin1));
  EXPECT_TRUE(result->GetOrigins().contains(origin2));
  result->AddNotificationPermission(NotificationPermissions(
      origin2, ContentSettingsPattern::Wildcard(), 456));
  EXPECT_EQ(2U, result->GetOrigins().size());
}

TEST_F(NotificationPermissionReviewServiceTest, ResultIsTrigger) {
  auto result = std::make_unique<
      NotificationPermissionsReviewService::NotificationPermissionsResult>();
  EXPECT_FALSE(result->IsTriggerForMenuNotification());
  result->AddNotificationPermission(NotificationPermissions(
      ContentSettingsPattern::FromString("https://example1.com:443"),
      ContentSettingsPattern::Wildcard(), 100));
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
}

TEST_F(NotificationPermissionReviewServiceTest, ResultWarrantsNewNotification) {
  auto origin1 = ContentSettingsPattern::FromString("https://example1.com:443");
  auto origin2 = ContentSettingsPattern::FromString("https://example2.com:443");
  auto old_result = std::make_unique<
      NotificationPermissionsReviewService::NotificationPermissionsResult>();
  auto new_result = std::make_unique<
      NotificationPermissionsReviewService::NotificationPermissionsResult>();
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result.get()->ToDictValue()));
  // origin1 revoked in new, but not in old -> warrants notification
  new_result->AddNotificationPermission(
      NotificationPermissions(origin1, ContentSettingsPattern::Wildcard(), 12));
  EXPECT_TRUE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 in both new and old -> no notification
  old_result->AddNotificationPermission(
      NotificationPermissions(origin1, ContentSettingsPattern::Wildcard(), 34));
  ;
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 in both, origin2 in new -> warrants notification
  new_result->AddNotificationPermission(
      NotificationPermissions(origin2, ContentSettingsPattern::Wildcard(), 56));
  EXPECT_TRUE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 and origin2 in both new and old -> no notification
  old_result->AddNotificationPermission(
      NotificationPermissions(origin2, ContentSettingsPattern::Wildcard(), 78));
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
}

TEST_F(NotificationPermissionReviewServiceTest, UpdateAsync) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(features::kSafetyHub);

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());

  // The result should be empty before we add test notifications.
  EXPECT_EQ(0U, GetNotificationPermissionsFromService().size());

  CreateMockNotificationPermissionsForReview();

  // The Safety Hub service will automatically update at the start, but there is
  // no observer that notifies its completion. Hence, another forced update is
  // made to avoid flaky tests.
  safety_hub_test_util::UpdateSafetyHubServiceAsync(service);

  // The result should be non empty after we update the service.
  std::vector<NotificationPermissions> notification_permissions =
      GetNotificationPermissionsFromService();
  EXPECT_EQ(2U, notification_permissions.size());

  // Sort notification permissions by number of notifications.
  std::sort(notification_permissions.begin(), notification_permissions.end(),
            [](const auto& left, const auto& right) {
              return left.notification_count > right.notification_count;
            });
  EXPECT_EQ("https://www.youtube.com:443",
            notification_permissions.front().primary_pattern.ToString());
  EXPECT_EQ(5, notification_permissions.front().notification_count);
  EXPECT_EQ("https://google.com:443",
            notification_permissions.back().primary_pattern.ToString());
  EXPECT_EQ(1, notification_permissions.back().notification_count);
}

TEST_F(NotificationPermissionReviewServiceTest, LatestResultInSync) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(features::kSafetyHub);

  // Create mock notifications before the service is started.
  CreateMockNotificationPermissionsForReview();

  // Start the service, and ensure that getting the latest results is run, and
  // that the two notifications are marked as in review.
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  base::RunLoop().RunUntilIdle();
  service->GetAsWeakRef();
  EXPECT_EQ(2U, GetNotificationPermissionsFromService().size());

  // When a notification permission is reset to ask, it shouldn't be part of the
  // latest result any more.
  hcsm()->SetContentSettingDefaultScope(GURL("https://google.com:443"), GURL(),
                                        ContentSettingsType::NOTIFICATIONS,
                                        CONTENT_SETTING_ASK);
  EXPECT_EQ(1U, GetNotificationPermissionsFromService().size());

  // When ignoring a site for review, it shouldn't be part of the latest result
  // any more.
  service->AddPatternToNotificationPermissionReviewBlocklist(
      ContentSettingsPattern::FromString("https://www.youtube.com:443"),
      ContentSettingsPattern::Wildcard());
  EXPECT_EQ(0U, GetNotificationPermissionsFromService().size());

  // Removing the site from the ignore list should include it again for review.
  service->RemovePatternFromNotificationPermissionReviewBlocklist(
      ContentSettingsPattern::FromString("https://www.youtube.com:443"),
      ContentSettingsPattern::Wildcard());
  EXPECT_EQ(1U, GetNotificationPermissionsFromService().size());

  // Blocking that site should again remove it from the list.
  hcsm()->SetContentSettingDefaultScope(
      GURL("https://www.youtube.com:443"), GURL(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(0U, GetNotificationPermissionsFromService().size());
}

TEST_F(NotificationPermissionReviewServiceTest,
       SetNotificationPermissionForOrigin) {
  auto pattern = ContentSettingsPattern::FromString("https://example1.com:443");

  // Check the permission for the origins is block.
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  service->SetNotificationPermissionsForOrigin(pattern.ToString(),
                                               CONTENT_SETTING_BLOCK);

  auto type = hcsm()->GetContentSetting(GURL(pattern.ToString()), GURL(),
                                        ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_BLOCK, type);

  // Check the permission for the origins is allow.
  service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  service->SetNotificationPermissionsForOrigin(pattern.ToString(),
                                               CONTENT_SETTING_ALLOW);

  type = hcsm()->GetContentSetting(GURL(pattern.ToString()), GURL(),
                                   ContentSettingsType::NOTIFICATIONS);

  // Check the permission for the origins is reset.
  service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  service->SetNotificationPermissionsForOrigin(pattern.ToString(),
                                               CONTENT_SETTING_DEFAULT);

  type = hcsm()->GetContentSetting(GURL(pattern.ToString()), GURL(),
                                   ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ASK, type);
}
