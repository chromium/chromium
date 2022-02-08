// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"

#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::message_center::Notification;

namespace {
constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";
constexpr char kKioskEmailId[] = "test-kiosk@example.com";
constexpr char kKioskGaiaId[] = "6789";
constexpr char kPendingRebootNotificationId[] =
    "ash.device_scheduled_reboot_pending_notification";
const base::Time::Exploded kRebootTime2022Feb2At1520 = {
    2022,  // year
    2,     // month
    4,     // day_of_week
    2,     // day_of_month
    15,    // hour
    20,    // minute
    0,     // second
    0      // millisecond
};
const base::Time::Exploded kRebootTime2023May15At1115 = {
    2023,  // year
    5,     // month
    4,     // day_of_week
    15,    // day_of_month
    11,    // hour
    15,    // minute
    0,     // second
    0      // millisecond
};
}  // namespace

class RebootNotificationControllerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    std::unique_ptr<ash::FakeChromeUserManager> fake_user_manager =
        std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

 protected:
  absl::optional<Notification> GetNotification() const {
    return display_service_tester_->GetNotification(
        kPendingRebootNotificationId);
  }

  int GetTransientNotificationCount() const {
    return display_service_tester_
        ->GetDisplayedNotificationsForType(NotificationHandler::Type::TRANSIENT)
        .size();
  }

  void CreateFakeUser(AccountId account_id) {
    profile_ = profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    fake_user_manager_->AddUser(account_id);
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
  }

  void CreateFakeKioskUser(AccountId account_id) {
    profile_ = profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    fake_user_manager_->AddKioskAppUser(account_id);
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
  }

  void CreateFakeMgsUser(AccountId account_id) {
    profile_ = profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    fake_user_manager_->AddPublicAccountUser(account_id);
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
  }

  void LoginFakeUser(AccountId account_id) {
    fake_user_manager_->LoginUser(account_id, true);
    EXPECT_EQ(ProfileManager::GetActiveUserProfile(), profile_);
  }

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_ = nullptr;

  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

TEST_F(RebootNotificationControllerTest, UserSessionShowsNotification) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  CreateFakeUser(account_id);
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notification.
  RebootNotificationController notification_controller;
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(absl::nullopt, GetNotification());

  // Log in user and show notification.
  LoginFakeUser(account_id);
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(absl::nullopt, GetNotification());
  EXPECT_EQ(
      GetNotification()->message(),
      u"Your administrator will restart your device at 3:20 PM on Feb 2, 2022");
}

TEST_F(RebootNotificationControllerTest, UserSessionNotificationChanged) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  CreateFakeUser(account_id);
  base::Time reboot_time1, reboot_time2;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time1));
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2023May15At1115, &reboot_time2));

  // User is not logged in. Don't show notification.
  RebootNotificationController notification_controller;
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time1, base::NullCallback());
  EXPECT_EQ(absl::nullopt, GetNotification());
  EXPECT_EQ(GetTransientNotificationCount(), 0);

  // Log in user and show notification.
  LoginFakeUser(account_id);
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time1, base::NullCallback());
  EXPECT_NE(absl::nullopt, GetNotification());
  EXPECT_EQ(
      GetNotification()->message(),
      u"Your administrator will restart your device at 3:20 PM on Feb 2, 2022");
  EXPECT_EQ(GetTransientNotificationCount(), 1);

  // Change reboot time. Close old notification and show new one.
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time2, base::NullCallback());
  EXPECT_NE(absl::nullopt, GetNotification());
  EXPECT_EQ(GetNotification()->message(),
            u"Your administrator will restart your device at 11:15 AM on May "
            u"15, 2023");
  EXPECT_EQ(GetTransientNotificationCount(), 1);
}

TEST_F(RebootNotificationControllerTest, ManagedGuestSessionShowsNotification) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  CreateFakeMgsUser(account_id);
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notification.
  RebootNotificationController notification_controller;
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(absl::nullopt, GetNotification());

  // Log in user and show notification.
  LoginFakeUser(account_id);
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(absl::nullopt, GetNotification());
  EXPECT_EQ(
      GetNotification()->message(),
      u"Your administrator will restart your device at 3:20 PM on Feb 2, 2022");
}

TEST_F(RebootNotificationControllerTest, KioskSessionDoesNotShowNotification) {
  AccountId account_id =
      AccountId::FromUserEmailGaiaId(kKioskEmailId, kKioskGaiaId);
  CreateFakeKioskUser(account_id);
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromUTCExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notification.
  RebootNotificationController notification_controller;
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(absl::nullopt, GetNotification());

  // Start kiosk session. Don't show notification.
  LoginFakeUser(account_id);
  notification_controller.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(absl::nullopt, GetNotification());
}
