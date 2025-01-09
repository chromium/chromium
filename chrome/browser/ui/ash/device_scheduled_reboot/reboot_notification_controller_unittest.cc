// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
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
constexpr base::Time::Exploded kRebootTime2022Feb2At1520 = {.year = 2022,
                                                            .month = 2,
                                                            .day_of_week = 4,
                                                            .day_of_month = 2,
                                                            .hour = 15,
                                                            .minute = 20};
constexpr base::Time::Exploded kRebootTime2023May15At1115 = {.year = 2023,
                                                             .month = 5,
                                                             .day_of_week = 4,
                                                             .day_of_month = 15,
                                                             .hour = 11,
                                                             .minute = 15};

class ClickCounter {
 public:
  void ButtonClickCallback() { ++clicks; }
  int clicks = 0;
  base::WeakPtrFactory<ClickCounter> weak_ptr_factory_{this};
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
  std::optional<Notification> GetPendingRebootNotification() const {
    return display_service_tester_->GetNotification(
        ash::kPendingRebootNotificationId);
  }

  std::optional<Notification> GetPostRebootNotification() const {
    return display_service_tester_->GetNotification(
        ash::kPostRebootNotificationId);
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
  raw_ptr<TestingProfile> profile_ = nullptr;

  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_ =
      nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  RebootNotificationController notification_controller_;
};

TEST_F(RebootNotificationControllerTest, UserSessionShowsNotification) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  CreateFakeUser(account_id);
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notifications.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(std::nullopt, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(std::nullopt, GetPostRebootNotification());

  // Log in user and show pending reboot notification.
  LoginFakeUser(account_id);
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(std::nullopt, GetPendingRebootNotification());
  EXPECT_EQ(GetPendingRebootNotification()->message(),
            u"Your administrator will restart your device at 3:20\u202fPM on "
            u"Feb 2, 2022");

  // Show post reboot notification.
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_NE(std::nullopt, GetPostRebootNotification());
  EXPECT_EQ(GetPostRebootNotification()->title(),
            u"Your administrator restarted your device");
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
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time1, base::NullCallback());
  EXPECT_EQ(std::nullopt, GetPendingRebootNotification());
  EXPECT_EQ(GetTransientNotificationCount(), 0);

  // Log in user and show notification.
  LoginFakeUser(account_id);
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time1, base::NullCallback());
  EXPECT_NE(std::nullopt, GetPendingRebootNotification());
  EXPECT_EQ(GetPendingRebootNotification()->message(),
            u"Your administrator will restart your device at 3:20\u202fPM on "
            u"Feb 2, 2022");
  EXPECT_EQ(GetTransientNotificationCount(), 1);

  // Change reboot time. Close old notification and show new one.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time2, base::NullCallback());
  EXPECT_NE(std::nullopt, GetPendingRebootNotification());
  EXPECT_EQ(
      GetPendingRebootNotification()->message(),
      u"Your administrator will restart your device at 11:15\u202fAM on May "
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
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(std::nullopt, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(std::nullopt, GetPostRebootNotification());

  // Log in user and show notification.
  LoginFakeUser(account_id);
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(std::nullopt, GetPendingRebootNotification());
  EXPECT_EQ(GetPendingRebootNotification()->message(),
            u"Your administrator will restart your device at 3:20\u202fPM on "
            u"Feb 2, 2022");

  // Show post reboot notification.
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_NE(std::nullopt, GetPostRebootNotification());
  EXPECT_EQ(GetPostRebootNotification()->title(),
            u"Your administrator restarted your device");
}

TEST_F(RebootNotificationControllerTest, KioskSessionDoesNotShowNotification) {
  AccountId account_id =
      AccountId::FromUserEmailGaiaId(kKioskEmailId, kKioskGaiaId);
  CreateFakeKioskUser(account_id);
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromUTCExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notifications.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(std::nullopt, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(std::nullopt, GetPostRebootNotification());

  // Start kiosk session. Don't show notifications.
  LoginFakeUser(account_id);
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(std::nullopt, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(std::nullopt, GetPostRebootNotification());
}

TEST_F(RebootNotificationControllerTest, CloseNotification) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  CreateFakeUser(account_id);
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // Log in user and show notification.
  LoginFakeUser(account_id);
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(std::nullopt, GetPendingRebootNotification());
  EXPECT_EQ(GetTransientNotificationCount(), 1);

  // Explicitly close notification.
  notification_controller_.CloseRebootNotification();
  EXPECT_EQ(std::nullopt, GetPendingRebootNotification());
  EXPECT_EQ(GetTransientNotificationCount(), 0);
}

TEST_F(RebootNotificationControllerTest, HandleNotificationClick) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  CreateFakeUser(account_id);
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // Log in user and show notification.
  LoginFakeUser(account_id);
  ClickCounter counter;
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::BindRepeating(&ClickCounter::ButtonClickCallback,
                                       counter.weak_ptr_factory_.GetWeakPtr()));
  auto notification = GetPendingRebootNotification().value();
  // Click on notification and do nothing.
  notification.delegate()->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(counter.clicks, 0);
  // Click on notification button and run callback.
  notification.delegate()->Click(0, std::nullopt);
  EXPECT_EQ(counter.clicks, 1);
}
