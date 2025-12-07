// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_result.h"

#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"

class NotificationPermissionReviewResultTest : public testing::Test {};

TEST_F(NotificationPermissionReviewResultTest, ToDict) {
  auto origin = ContentSettingsPattern::FromString("https://example1.com:443");
  const int notification_count = 1337;

  auto result = std::make_unique<NotificationPermissionsReviewResult>();
  result->AddNotificationPermission(NotificationPermissions(
      origin, ContentSettingsPattern::Wildcard(), notification_count));
  EXPECT_THAT(result->GetOrigins(), testing::ElementsAre(origin));

  // When converting to dict, the values of the notification permissions should
  // be correctly converted to base::Value.
  base::Value::Dict dict = result->ToDictValue();
  auto* notification_perms_list =
      dict.FindList(kSafetyHubNotificationPermissionsReviewResultKey);
  EXPECT_EQ(1U, notification_perms_list->size());

  base::Value::Dict& notification_perm =
      notification_perms_list->front().GetDict();
  EXPECT_EQ(origin.ToString(),
            *notification_perm.FindString(kSafetyHubOriginKey));
}

TEST_F(NotificationPermissionReviewResultTest, GetOrigins) {
  auto origin1 = ContentSettingsPattern::FromString("https://example1.com:443");
  auto origin2 = ContentSettingsPattern::FromString("https://example2.com:443");
  auto result = std::make_unique<NotificationPermissionsReviewResult>();
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

TEST_F(NotificationPermissionReviewResultTest, IsTrigger) {
  auto result = std::make_unique<NotificationPermissionsReviewResult>();
  EXPECT_FALSE(result->IsTriggerForMenuNotification());
  result->AddNotificationPermission(NotificationPermissions(
      ContentSettingsPattern::FromString("https://example1.com:443"),
      ContentSettingsPattern::Wildcard(), 100));
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
}

TEST_F(NotificationPermissionReviewResultTest, WarrantsNewNotification) {
  auto origin1 = ContentSettingsPattern::FromString("https://example1.com:443");
  auto origin2 = ContentSettingsPattern::FromString("https://example2.com:443");
  auto old_result = std::make_unique<NotificationPermissionsReviewResult>();
  auto new_result = std::make_unique<NotificationPermissionsReviewResult>();
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
