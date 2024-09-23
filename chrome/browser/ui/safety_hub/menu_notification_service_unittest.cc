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
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/constants.h"
#include "components/permissions/pref_names.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
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
    feature_list_.InitWithFeatures(
        {
            features::kSafetyHub,
            safe_browsing::kSafetyHubAbusiveNotificationRevocation,
#if BUILDFLAG(IS_ANDROID)
            features::kSafetyHubFollowup,
#else
            features::kSafetyHubHaTSOneOffSurvey,
#endif
        },
        {});
    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);
#if !BUILDFLAG(IS_ANDROID)
    // mock_hats_service_ should return true for CanShowAnySurvey on each test
    // running for desktop, since hats service is called in
    // SafetyHubMenuNotificationService ctor.
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
#endif
  }

  void TearDown() override {
    // Wait till all ongoing tasks to be finalized to let password manager
    // enough time to complete password checks.
    RunUntilIdle();
    mock_hats_service_ = nullptr;
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
            UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
                ContentSettingsType::GEOLOCATION)));
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()));
    safety_hub_test_util::UpdateSafetyHubServiceAsync(
        unused_site_permissions_service());
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

  UnusedSitePermissionsService* unused_site_permissions_service() {
    return UnusedSitePermissionsServiceFactory::GetForProfile(profile());
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
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
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
  raw_ptr<MockHatsService> mock_hats_service_;
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
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::SAFE_BROWSING);

  // A leaked password warning should override the Safe Browsing notification.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 1);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1,
      notification.value().label);
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::PASSWORDS);

  // Fixing the leaked password will clear notification. Because the safe
  // browsing notification was dismissed, it will not be shown either.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 0);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // The last shown menu notification remains the same even when it has been
  // dismissed.
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::PASSWORDS);
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
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());

  // A leaked password count of lower value should NOT create a new password
  // notification.
  prefs()->SetInteger(prefs::kBreachedCredentialsCount, 1);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());

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
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);

  // When all notifications are dismissed, there should be no more notification
  // but the last shown notification remains the same.
  menu_notification_service()->DismissActiveNotification();
  EXPECT_FALSE(
      menu_notification_service()->GetNotificationToShow().has_value());
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);
}

// TODO(crbug.com/328773301): Remove after
// SafetyHubAbusiveNotificationRevocation is launched.
class
    SafetyHubMenuNotificationServiceTestDisableAutoAbusiveNotificationRevocation
    : public SafetyHubMenuNotificationServiceTest {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitWithFeatures(
        {features::kSafetyHub},
        {safe_browsing::kSafetyHubAbusiveNotificationRevocation});
    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    SafetyHubMenuNotificationServiceTestDisableAutoAbusiveNotificationRevocation,
    TwoNotificationsSequentially) {
  // Creating a mock result, which should result in a notification to be
  // available.
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");

  // Show the notification sufficient days and times.
  std::optional<MenuNotificationEntry> notification;
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

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/328773301): Remove after
// SafetyHubAbusiveNotificationRevocation is launched.
class SafetyHubMenuNotificationServiceDesktopOnlyTest
    : public SafetyHubMenuNotificationServiceTest {
 public:
  void SetUp() override {
    SafetyHubMenuNotificationServiceTest::SetUp();

    password_store_ = CreateAndUseTestPasswordStore(profile());
    PasswordStatusCheckService* password_service =
        PasswordStatusCheckServiceFactory::GetForProfile(profile());
    RunUntilIdle();
    EXPECT_EQ(password_service->compromised_credential_count(), 0UL);
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

  PasswordStatusCheckService* password_status_check_service() {
    return PasswordStatusCheckServiceFactory::GetForProfile(profile());
  }
  extensions::CWSInfoService* extension_info_service() {
    return extensions::CWSInfoService::Get(profile());
  }
  password_manager::TestPasswordStore& password_store() {
    return *password_store_;
  }

  void CreateMockCWSInfoService() {
    extensions::CWSInfoServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return safety_hub_test_util::GetMockCWSInfoService(
              Profile::FromBrowserContext(context));
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<password_manager::TestPasswordStore> password_store_;
};

TEST_F(SafetyHubMenuNotificationServiceDesktopOnlyTest,
       ExtensionsMenuNotification) {
  // Create mock extensions that should result in two violations that are shown
  // in the menu notification.
  safety_hub_test_util::CreateMockExtensions(profile());
  // Create a mock CWS info service that will return the information that
  // matches the mock extensions.
  CreateMockCWSInfoService();
  // Create a menu notification service with the mocked CWS info service.
  std::unique_ptr<SafetyHubMenuNotificationService> mocked_service =
      std::make_unique<SafetyHubMenuNotificationService>(
          prefs(), unused_site_permissions_service(),
          notification_permissions_service(), password_status_check_service(),
          profile());
  std::optional<MenuNotificationEntry> notification =
      mocked_service->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MENU_NOTIFICATION, 2,
                     notification->label);
}

TEST_F(SafetyHubMenuNotificationServiceDesktopOnlyTest, PasswordOverride) {
  const std::string origin = "https://www.example.com";
  std::optional<MenuNotificationEntry> notification;
  // Show Safe Browsing notification.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  notification = menu_notification_service()->GetNotificationToShow();
  AdvanceClockBy(base::Days(1));
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::SAFE_BROWSING);

  // A leaked password warning should override the safe browsing notification.
  SetMockCredentialEntry(origin, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1,
      notification.value().label);
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::PASSWORDS);

  // Fixing the leaked password will clear notification. Because the safe
  // browsing notification was dismissed, it will not be shown either.
  SetMockCredentialEntry(origin, false, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());

  // The last shown menu notification remains the same even when it has been
  // dismissed.
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::PASSWORDS);
}

TEST_F(SafetyHubMenuNotificationServiceDesktopOnlyTest, PasswordTrigger) {
  const std::string& origin = "https://www.example1.com";
  // A leaked password warning should create a password notification.
  std::optional<MenuNotificationEntry> notification;
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

TEST_F(SafetyHubMenuNotificationServiceDesktopOnlyTest,
       DismissPasswordNotification) {
  std::optional<MenuNotificationEntry> notification;
  // Create mock password menu notification.
  const std::string& kOrigin = "https://www.example.com";
  SetMockCredentialEntry(kOrigin, true);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  ExpectPluralString(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1,
      notification.value().label);
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::PASSWORDS);

  // The notification should no longer appear after it has been dismissed.
  menu_notification_service()->DismissActiveNotificationOfModule(
      safety_hub::SafetyHubModuleType::PASSWORDS);
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
  EXPECT_TRUE(menu_notification_service()
                  ->GetLastShownNotificationModule()
                  .has_value());
  EXPECT_EQ(
      menu_notification_service()->GetLastShownNotificationModule().value(),
      safety_hub::SafetyHubModuleType::PASSWORDS);
}

TEST_F(SafetyHubMenuNotificationServiceDesktopOnlyTest, PasswordMigration) {
  const std::string& kOrigin = "https://www.example1.com";
  // Add a leaked password to make prefs store password data.
  SetMockCredentialEntry(kOrigin, true);
  EXPECT_TRUE(menu_notification_service()->GetNotificationToShow().has_value());

  // Modify stored password data on prefs to test migration from old state to
  // new state.
  const base::Value::Dict& stored_notifications =
      prefs()->GetDict(safety_hub_prefs::kMenuNotificationsPrefsKey);
  base::Value::Dict new_stored_notification(stored_notifications.Clone());
  const base::Value::Dict* stored_password_data =
      new_stored_notification.FindDict("passwords");

  // Store notification password as its old format by only passing origin.
  base::Value::Dict old_password_not_value;
  base::Value::List compromised_origins;
  compromised_origins.Append(kOrigin);
  old_password_not_value.Set(safety_hub::kSafetyHubPasswordCheckOriginsKey,
                             std::move(compromised_origins));
  base::Value::Dict new_stored_password_data(stored_password_data->Clone());
  new_stored_password_data.Set("result", std::move(old_password_not_value));

  // Update the value stored on prefs
  new_stored_notification.Set("passwords", std::move(new_stored_password_data));
  prefs()->SetDict(safety_hub_prefs::kMenuNotificationsPrefsKey,
                   std::move(new_stored_notification));
  EXPECT_TRUE(menu_notification_service()->GetNotificationToShow().has_value());

  // Adding a new notification with the new format should keep showing the
  // notification.
  SetMockCredentialEntry(kOrigin, true);
  EXPECT_TRUE(menu_notification_service()->GetNotificationToShow().has_value());
}

TEST_F(SafetyHubMenuNotificationServiceDesktopOnlyTest, HaTSControlTriggerNew) {
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerSafetyHubOneOffExperimentControl,
                           _, _, _, _))
      .Times(1);
  // Creating service without any notification should trigger survey for control
  // group for A/B experiment.
  std::optional<MenuNotificationEntry> notification =
      menu_notification_service()->GetNotificationToShow();

  // After a notification is shown, control group should not be triggered for
  // A/B experiment.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerSafetyHubOneOffExperimentControl,
                           _, _, _, _))
      .Times(0);
  CreateMockUnusedSitePermissionsEntry("https://example1.com:443");
  // The notification to show should be the unused site permissions one with
  // one revoked permission. The relevant command should be to open Safety Hub.
  notification = menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
}
#endif  // !BUILDFLAG(IS_ANDROID)
