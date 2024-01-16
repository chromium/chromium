// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

#include <ctime>
#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/permissions/constants.h"
#include "components/permissions/pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

class SafetyHubMenuNotificationServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SafetyHubMenuNotificationServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitWithFeatures({features::kSafetyHub}, {});
    prefs()->SetBoolean(
        permissions::prefs::kUnusedSitePermissionsRevocationEnabled, true);

    password_store_ = CreateAndUseTestPasswordStore(profile());
    PasswordStatusCheckService* password_service =
        PasswordStatusCheckServiceFactory::GetForProfile(profile());
    RunUntilIdle();
    EXPECT_EQ(password_service->compromised_credential_count(), 0UL);
  }

  void TearDown() override {
    // Wait till all ongoing tasks to be finalized to let password manager
    // enough time to complete password checks.
    RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // TODO(crbug.com/1443466): Replace this with password_service specific
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

  void CreateMockUnusedSitePermissionsEntry() {
    // Revoke permission and update the unused site permission service.
    const std::string url1 = "https://example1.com:443";
    auto dict = base::Value::Dict().Set(
        permissions::kRevokedKey,
        base::Value::List().Append(
            static_cast<int32_t>(ContentSettingsType::GEOLOCATION)));
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url1), GURL(url1),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()));
    safety_hub_test_util::UpdateSafetyHubServiceAsync(
        unused_site_permissions_service());
  }

  void SetMockCredentialEntry(const std::string origin,
                              bool leaked,
                              bool update = false) {
    // Create a password form and mark it as leaked.
    password_manager::PasswordForm form;
    form.username_value = u"username";
    form.password_value = u"password";
    form.signon_realm = origin;
    form.url = GURL(origin);

    if (leaked) {
      form.password_issues.insert_or_assign(
          password_manager::InsecureType::kLeaked,
          password_manager::InsecurityMetadata(
              base::Time::Now(), password_manager::IsMuted(false),
              password_manager::TriggerBackendNotification(false)));
    }

    if (update) {
      password_store().UpdateLogin(form);
    } else {
      password_store().AddLogin(form);
    }
    RunUntilIdle();
  }

  void ShowNotificationEnoughTimes(
      int remainingImpressionCount =
          kSafetyHubMenuNotificationMinImpressionCount) {
    absl::optional<MenuNotificationEntry> notification;
    AdvanceClockBy(base::Days(90));
    for (int i = 0; i < remainingImpressionCount; ++i) {
      notification = menu_notification_service()->GetNotificationToShow();
      EXPECT_TRUE(notification.has_value());
    }
    AdvanceClockBy(kSafetyHubMenuNotificationMinNotificationDuration);
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_FALSE(notification.has_value());
  }

  UnusedSitePermissionsService* unused_site_permissions_service() {
    return UnusedSitePermissionsServiceFactory::GetForProfile(profile());
  }
  NotificationPermissionsReviewService* notification_permissions_service() {
    return NotificationPermissionsReviewServiceFactory::GetForProfile(
        profile());
  }
  PasswordStatusCheckService* password_status_check_service() {
    return PasswordStatusCheckServiceFactory::GetForProfile(profile());
  }
  SafetyHubMenuNotificationService* menu_notification_service() {
    return SafetyHubMenuNotificationServiceFactory::GetForProfile(profile());
  }
  extensions::CWSInfoService* extension_info_service() {
    return extensions::CWSInfoService::Get(profile());
  }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
  password_manager::TestPasswordStore& password_store() {
    return *password_store_;
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

 private:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<password_manager::TestPasswordStore> password_store_;
};

TEST_F(SafetyHubMenuNotificationServiceTest, GetNotificationToShowNoResult) {
  absl::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, SingleNotificationToShow) {
  CreateMockUnusedSitePermissionsEntry();

  // The notification to show should be the unused site permissions one with
  // one revoked permission. The relevant command should be to open Safety Hub.
  absl::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification.value().label);
  EXPECT_EQ(IDC_OPEN_SAFETY_HUB, notification.value().command);
}

TEST_F(SafetyHubMenuNotificationServiceTest, PersistInPrefs) {
  // Creating a mock result, which should result in a notification to be
  // available.
  CreateMockUnusedSitePermissionsEntry();

  absl::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  SafetyHubMenuNotification* old_notification =
      menu_notification_service()->GetNotificationForTesting(
          safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);
  EXPECT_TRUE(old_notification->IsCurrentlyActive());
  auto* old_result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          old_notification->GetResultForTesting());
  EXPECT_EQ(1U, old_result->GetRevokedPermissions().size());

  // After |GetNotificationToShow()| was called, the notification should be
  // persisted in the prefs. When creating a new service, that result should be
  // loaded in memory.
  std::unique_ptr<SafetyHubMenuNotificationService> new_service =
      std::make_unique<SafetyHubMenuNotificationService>(
          prefs(), unused_site_permissions_service(),
          notification_permissions_service(), extension_info_service(),
          password_status_check_service(), profile());
  // Getting the in-memory notification to prevent the service from generating a
  // new one.
  SafetyHubMenuNotification* new_notification =
      new_service->GetNotificationForTesting(
          safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);
  EXPECT_TRUE(new_notification->IsCurrentlyActive());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
      new_notification->GetNotificationString());
  auto* new_result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          new_notification->GetResultForTesting());

  EXPECT_EQ(old_result->GetRevokedPermissions().size(),
            new_result->GetRevokedPermissions().size());
  EXPECT_EQ(old_result->GetRevokedPermissions().front().origin,
            new_result->GetRevokedPermissions().front().origin);
  EXPECT_EQ(old_result->GetRevokedPermissions().front().expiration,
            new_result->GetRevokedPermissions().front().expiration);
  EXPECT_EQ(old_result->GetRevokedPermissions().front().permission_types,
            new_result->GetRevokedPermissions().front().permission_types);
}

TEST_F(SafetyHubMenuNotificationServiceTest, TwoNotificationsSequentially) {
  // Creating a mock result, which should result in a notification to be
  // available.
  CreateMockUnusedSitePermissionsEntry();

  // Show the notification sufficient days and times.
  absl::optional<MenuNotificationEntry> notification;
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount; ++i) {
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_TRUE(notification.has_value());
    ExpectPluralString(
        IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
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
  CreateMockUnusedSitePermissionsEntry();

  // Show the notification once.
  absl::optional<MenuNotificationEntry> notification;
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification->label);

  // Creating a notification permission shouldn't cause the active notification
  // to be overridden.
  CreateMockNotificationPermissionEntry();
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification->label);

  // Showing the notification sufficient days and times.
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount - 2; ++i) {
    notification = menu_notification_service()->GetNotificationToShow();
    EXPECT_TRUE(notification.has_value());
    ExpectPluralString(
        IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
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
  CreateMockUnusedSitePermissionsEntry();
  absl::optional<MenuNotificationEntry> notification;
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
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
  absl::optional<MenuNotificationEntry> notification;
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

TEST_F(SafetyHubMenuNotificationServiceTest, ExtensionsMenuNotification) {
  // Create mock extensions that should result in two violations that are shown
  // in the menu notification.
  safety_hub_test_util::CreateMockExtensions(profile());
  // The mock CWS info service ensures that the correct extension properties are
  // provided.
  std::unique_ptr<testing::NiceMock<safety_hub_test_util::MockCWSInfoService>>
      cws_info_service = safety_hub_test_util::GetMockCWSInfoService(profile());
  // Create a menu notification service with the mocked CWS info service.
  std::unique_ptr<SafetyHubMenuNotificationService> mocked_service =
      std::make_unique<SafetyHubMenuNotificationService>(
          prefs(), unused_site_permissions_service(),
          notification_permissions_service(), cws_info_service.get(),
          password_status_check_service(), profile());
  absl::optional<MenuNotificationEntry> notification =
      mocked_service->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MENU_NOTIFICATION, 2,
                     notification->label);
}

TEST_F(SafetyHubMenuNotificationServiceTest, PasswordOverride) {
  const std::string origin = "https://www.example.com";
  absl::optional<MenuNotificationEntry> notification;
  // Disabling Safe Browsing should only trigger a menu notification after one
  // day.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
  AdvanceClockBy(base::Days(1));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());

  // A leaked password warning should override the safe browsing notification.
  SetMockCredentialEntry(origin, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1,
      notification.value().label);

  // Fixing the leaked password will clear notification. Because the safe
  // browsing notification was dismissed, it will not be shown either.
  SetMockCredentialEntry(origin, false, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, PasswordTrigger) {
  const std::string& origin = "https://www.example1.com";
  // A leaked password warning should create a password notification.
  absl::optional<MenuNotificationEntry> notification;
  SetMockCredentialEntry(origin, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1,
      notification.value().label);

  // Fixing the leaked password will clear notification.
  SetMockCredentialEntry(origin, false, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, DismissNotifications) {
  // Generate a mock notification for unused site permissions.
  CreateMockUnusedSitePermissionsEntry();
  absl::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1,
      notification.value().label);

  // When all notifications are dismissed, there should be no more notification.
  menu_notification_service()->DismissActiveNotification();
  EXPECT_FALSE(
      menu_notification_service()->GetNotificationToShow().has_value());

  // Create mock password menu notification.
  const std::string& kOrigin = "https://www.example.com";
  SetMockCredentialEntry(kOrigin, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1,
      notification.value().label);

  // The notification should no longer appear after it has been dismissed.
  menu_notification_service()->DismissActiveNotificationOfModule(
      safety_hub::SafetyHubModuleType::PASSWORDS);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}
