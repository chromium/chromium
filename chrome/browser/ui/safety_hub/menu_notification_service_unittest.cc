// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

#include <ctime>
#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/constants.h"
#include "components/permissions/pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service_factory.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#endif  // BUILDFLAG(IS_ANDROID)

using ::testing::_;

class SafetyHubMenuNotificationServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SafetyHubMenuNotificationServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);

    safety_hub_test_util::CreateRevokedPermissionsService(profile());
    safety_hub_test_util::CreateNotificationPermissionsReviewService(profile());
  }

  void TearDown() override {
    // Wait till all ongoing tasks to be finalized to let password manager
    // enough time to complete password checks.
    RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // TODO(crbug.com/40267370): Replace this with password_service specific
  // RunUntilIdle.
  void RunUntilIdle() { task_environment()->RunUntilIdle(); }

 protected:
  void CreateMockNotificationPermissionEntry() {
    const GURL url = GURL("https://example.com:443");
    hcsm()->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(profile());

    // For simplicity, not setting an engagement score as that implies a NONE
    // engagement level, and will mark the site for review of notification
    // permissions.
    notifications_engagement_service->RecordNotificationDisplayed(url, 7);
    safety_hub_test_util::UpdateSafetyHubServiceAsync(
        notification_permissions_service());
  }

  void CreateMockUnusedSitePermissionsEntry(const std::string url) {
    // Revoke permission and update the unused site permission service.
    auto dict = base::Value::Dict().Set(
        permissions::kRevokedKey,
        base::Value::List().Append(
            UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
                ContentSettingsType::GEOLOCATION)));
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()));
    safety_hub_test_util::UpdateSafetyHubServiceAsync(
        revoked_permissions_service());
  }

  void ShowNotificationEnoughTimes(
      int remainingImpressionCount =
          kSafetyHubMenuNotificationMinImpressionCount) {
    std::optional<MenuNotificationEntry> notification;
    AdvanceClockBy(base::Days(90));
    for (int i = 0; i < remainingImpressionCount; ++i) {
      notification = menu_notification_service()->GetNotificationToShow();
      EXPECT_TRUE(notification.has_value());
    }
    AdvanceClockBy(kSafetyHubMenuNotificationMinNotificationDuration);
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_FALSE(notification.has_value());
  }

  RevokedPermissionsService* revoked_permissions_service() {
    return RevokedPermissionsServiceFactory::GetForProfile(profile());
  }
  NotificationPermissionsReviewService* notification_permissions_service() {
    return NotificationPermissionsReviewServiceFactory::GetForProfile(
        profile());
  }
  SafetyHubMenuNotificationService* menu_notification_service() {
    return SafetyHubMenuNotificationServiceFactory::GetForProfile(profile());
  }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
  // Using |AdvanceClockBy| when the timers are not required to execute.
  void AdvanceClockBy(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
  }
  void ExpectPluralString(int string_id,
                          int count,
                          std::u16string notification_string) {
    EXPECT_EQ(l10n_util::GetPluralStringFUTF16(string_id, count),
              notification_string);
  }
};

TEST_F(SafetyHubMenuNotificationServiceTest, GetNotificationToShowNoResult) {
  std::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, SingleNotificationToShow) {
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");

  // The notification to show should be the unused site permissions one with
  // one revoked permission. The relevant command should be to open Safety Hub.
  std::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification.value().label);
  EXPECT_EQ(IDC_OPEN_SAFETY_HUB, notification.value().command);
}

TEST_F(SafetyHubMenuNotificationServiceTest, TwoNotificationsIncremental) {
  // Create a mock notification for example1.com, and show it sufficiently.
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");
  std::optional<MenuNotificationEntry> notification;
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount; ++i) {
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_TRUE(notification.has_value());
    ExpectPluralString(
        IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
        notification->label);
  }
  AdvanceClockBy(kSafetyHubMenuNotificationMinNotificationDuration);

  // The notification has been shown sufficiently, so shouldn't be shown again.
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // Even after the interval has passed, no notification should be shown.
  const base::TimeDelta kNotificationIntervalUnusedSitePermissions =
      base::Days(10);
  AdvanceClockBy(kNotificationIntervalUnusedSitePermissions);

  // The notification has been shown sufficiently, so shouldn't be shown again.
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // Create a mock notification for the same example1.com, and a new one for
  // example2.com. Because of this new one, there now should be a new
  // notification.
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");
  CreateMockUnusedSitePermissionsEntry("https://example2.com:443");
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, TwoNotificationsSequentially) {
  // Creating a mock result, which should result in a notification to be
  // available.
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");

  // Show the notification sufficient days and times.
  std::optional<MenuNotificationEntry> notification;
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount; ++i) {
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_TRUE(notification.has_value());
    ExpectPluralString(
        IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
        notification->label);
  }
  AdvanceClockBy(kSafetyHubMenuNotificationMinNotificationDuration);

  // The notification has been shown sufficiently, so shouldn't be shown again.
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  CreateMockNotificationPermissionEntry();
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, TwoNotificationsNoOverride) {
  // Creating a mock result, which should result in a notification to be
  // available.
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");

  // Show the notification once.
  std::optional<MenuNotificationEntry> notification;
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification->label);

  // Creating a notification permission shouldn't cause the active notification
  // to be overridden.
  CreateMockNotificationPermissionEntry();
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification->label);

  // Showing the notification sufficient days and times.
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount - 2; ++i) {
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_TRUE(notification.has_value());
    ExpectPluralString(
        IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
        notification->label);
  }
  AdvanceClockBy(kSafetyHubMenuNotificationMinNotificationDuration);

  // After the unused site permissions notification has been shown sufficient
  // times, the notification permission review notification should be shown.
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_REVIEW_NOTIFICATION_PERMISSIONS_MENU_NOTIFICATION,
      1, notification->label);

  // Showing the new notification enough times and days.
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount - 1; ++i) {
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_TRUE(notification.has_value());
    ExpectPluralString(
        IDS_SETTINGS_SAFETY_HUB_REVIEW_NOTIFICATION_PERMISSIONS_MENU_NOTIFICATION,
        1, notification->label);
  }
  AdvanceClockBy(kSafetyHubMenuNotificationMinNotificationDuration);

  // Both notifications have been shown sufficiently, so no new notification
  // should be shown.
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, SafeBrowsingOverride) {
  // Create a notification for a module that has low priority notifications.
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");
  std::optional<MenuNotificationEntry> notification;
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification->label);

  // Disable safe browsing, which generates a medium-priority Safe Browsing
  // notification that should override the low priority notification.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  AdvanceClockBy(base::Days(1));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MENU_NOTIFICATION),
            notification.value().label);

  // Re-enabling Safe Browsing should clear the notification. Because the unused
  // site permission notification was dismissed, it will not be shown either.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, SafeBrowsingTriggerLogic) {
  std::optional<MenuNotificationEntry> notification;
  // Disabling Safe Browsing should only trigger a menu notification after one
  // day.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  AdvanceClockBy(base::Hours(12));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
  AdvanceClockBy(base::Hours(12));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());

  // A notification for Safe Browsing should only be shown three times in total.
  ShowNotificationEnoughTimes(kSafetyHubMenuNotificationMinImpressionCount - 1);
  AdvanceClockBy(base::Days(90));
  ShowNotificationEnoughTimes();
  AdvanceClockBy(base::Days(90));
  ShowNotificationEnoughTimes();
  AdvanceClockBy(base::Days(90));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // When the user toggles the SB prefs, the notification can be shown again,
  // after one day.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  AdvanceClockBy(base::Days(1));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(SafetyHubMenuNotificationServiceTest, PasswordOverride) {
  std::optional<MenuNotificationEntry> notification;
  // Show Safe Browsing notification.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  notification = menu_notification_service()->GetNotificationToShow();
  AdvanceClockBy(base::Days(1));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());

  // A leaked password warning should override the Safe Browsing notification.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 1);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1,
      notification.value().label);

  // Fixing the leaked password will clear notification. Because the safe
  // browsing notification was dismissed, it will not be shown either.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 0);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, PasswordTrigger) {
  // If the leaked password count is not yet fetched or the user is signed out,
  // no notification should be displayed.
  std::optional<MenuNotificationEntry> notification;
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, -1);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // A leaked password warning should create a password notification.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 2);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 2,
      notification.value().label);

  // The notification should no longer appear after it has been dismissed.
  menu_notification_service()->DismissActiveNotificationOfModule(
      safety_hub::SafetyHubModuleType::PASSWORDS);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // A leaked password count of lower value should NOT create a new password
  // notification.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 1);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // A leaked password count of higher value should create a new password
  // notification.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 3);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 3,
      notification.value().label);

  // Fixing the leaked passwords should clear notification.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 0);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}
#endif

TEST_F(SafetyHubMenuNotificationServiceTest, DismissNotifications) {
  // Generate a mock notification for unused site permissions.
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");
  std::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification.value().label);

  // When all notifications are dismissed, there should be no more notification
  // but the last shown notification remains the same.
  menu_notification_service()->DismissActiveNotification();
  EXPECT_FALSE(
      menu_notification_service()->GetNotificationToShow().has_value());
}
