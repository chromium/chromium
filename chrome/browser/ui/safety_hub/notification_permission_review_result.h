// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_RESULT_H_

#include <set>

#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

inline constexpr char kSafetyHubNotificationInfoString[] =
    "notificationInfoString";
inline constexpr char kSafetyHubNotificationPermissionsReviewResultKey[] =
    "notificationPermissions";

struct NotificationPermissions {
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  int notification_count;

  NotificationPermissions(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          int notification_count);
  ~NotificationPermissions();
};

// The result of the periodic update contains the sites that sent a large
// number of notifications, along with the number of notifications that they
// sent. The sites that are added to the review blocklist should not be added
// here.
class NotificationPermissionsReviewResult : public SafetyHubResult {
 public:
  NotificationPermissionsReviewResult();

  NotificationPermissionsReviewResult(
      const NotificationPermissionsReviewResult&);
  NotificationPermissionsReviewResult& operator=(
      const NotificationPermissionsReviewResult&) = default;

  ~NotificationPermissionsReviewResult() override;

  void AddNotificationPermission(const NotificationPermissions&);
  std::vector<NotificationPermissions> GetSortedNotificationPermissions();
  base::Value::List GetSortedListValueForUI();

  // SafetyHubResult implementation
  base::Value::Dict ToDictValue() const override;
  bool IsTriggerForMenuNotification() const override;
  bool WarrantsNewMenuNotification(
      const base::Value::Dict& previous_result_dict) const override;
  std::u16string GetNotificationString() const override;
  int GetNotificationCommandId() const override;
  std::unique_ptr<SafetyHubResult> Clone() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationPermissionReviewResultTest, ToDict);
  FRIEND_TEST_ALL_PREFIXES(NotificationPermissionReviewResultTest, GetOrigins);

  std::set<ContentSettingsPattern> GetOrigins() const;

  std::vector<NotificationPermissions> notification_permissions_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_RESULT_H_
