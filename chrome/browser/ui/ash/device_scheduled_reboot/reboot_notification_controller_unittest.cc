// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

using ::message_center::Notification;

namespace {
constexpr char kEmailId[] = "test@example.com";
constexpr GaiaId::Literal kGaiaId("12345");
constexpr char kKioskId[] = "test-kiosk";
constexpr char kMgsId[] = "test-mgs";
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
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state());
    message_center::MessageCenter::Initialize();
  }

 protected:
  void TearDown() override {
    message_center::MessageCenter::Shutdown();
    user_ = nullptr;
    user_session_test_environment_.reset();
  }

  const Notification* GetNotification(
      const std::string& notification_id) const {
    return message_center::MessageCenter::Get()->FindNotificationById(
        ash::CreateUserScopedNotificationId(notification_id,
                                            user_->username_hash()));
  }

  const Notification* GetPendingRebootNotification() const {
    return GetNotification(ash::kPendingRebootNotificationId);
  }

  const Notification* GetPostRebootNotification() const {
    return GetNotification(ash::kPostRebootNotificationId);
  }

  int GetNotificationCount() const {
    return message_center::MessageCenter::Get()->GetNotifications().size();
  }

  void CreateFakeUser() {
    user_ = user_session_test_environment_->AddRegularUser(
        AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId));
    ASSERT_TRUE(user_);
  }

  void CreateFakeKioskUser() {
    user_ = user_session_test_environment_->AddKioskChromeAppUser(
        policy::GenerateDeviceLocalAccountUserId(
            kKioskId, policy::DeviceLocalAccountType::kKioskApp));
    ASSERT_TRUE(user_);
  }

  void CreateFakeMgsUser() {
    user_ = user_session_test_environment_->AddPublicAccountUser(
        policy::GenerateDeviceLocalAccountUserId(
            kMgsId, policy::DeviceLocalAccountType::kPublicSession));
    ASSERT_TRUE(user_);
  }

  void LoginFakeUser() {
    user_session_test_environment_->LogIn(user_->GetAccountId());
  }

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<const user_manager::User> user_ = nullptr;
  RebootNotificationController notification_controller_;
};

TEST_F(RebootNotificationControllerTest, UserSessionShowsNotification) {
  CreateFakeUser();
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notifications.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(nullptr, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(nullptr, GetPostRebootNotification());

  // Log in user and show pending reboot notification.
  LoginFakeUser();
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(nullptr, GetPendingRebootNotification());
  EXPECT_EQ(GetPendingRebootNotification()->message(),
            u"Your administrator will restart your device at 3:20\u202fPM on "
            u"Feb 2, 2022");

  // Show post reboot notification.
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_NE(nullptr, GetPostRebootNotification());
  EXPECT_EQ(GetPostRebootNotification()->title(),
            u"Your administrator restarted your device");
}

TEST_F(RebootNotificationControllerTest, UserSessionNotificationChanged) {
  CreateFakeUser();
  base::Time reboot_time1, reboot_time2;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time1));
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2023May15At1115, &reboot_time2));

  // User is not logged in. Don't show notification.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time1, base::NullCallback());
  EXPECT_EQ(nullptr, GetPendingRebootNotification());
  EXPECT_EQ(GetNotificationCount(), 0);

  // Log in user and show notification.
  LoginFakeUser();
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time1, base::NullCallback());
  EXPECT_NE(nullptr, GetPendingRebootNotification());
  EXPECT_EQ(GetPendingRebootNotification()->message(),
            u"Your administrator will restart your device at 3:20\u202fPM on "
            u"Feb 2, 2022");
  EXPECT_EQ(GetNotificationCount(), 1);

  // Change reboot time. Close old notification and show new one.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time2, base::NullCallback());
  EXPECT_NE(nullptr, GetPendingRebootNotification());
  EXPECT_EQ(
      GetPendingRebootNotification()->message(),
      u"Your administrator will restart your device at 11:15\u202fAM on May "
      u"15, 2023");
  EXPECT_EQ(GetNotificationCount(), 1);
}

TEST_F(RebootNotificationControllerTest, ManagedGuestSessionShowsNotification) {
  CreateFakeMgsUser();
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notification.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(nullptr, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(nullptr, GetPostRebootNotification());

  // Log in user and show notification.
  LoginFakeUser();
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(nullptr, GetPendingRebootNotification());
  EXPECT_EQ(GetPendingRebootNotification()->message(),
            u"Your administrator will restart your device at 3:20\u202fPM on "
            u"Feb 2, 2022");

  // Show post reboot notification.
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_NE(nullptr, GetPostRebootNotification());
  EXPECT_EQ(GetPostRebootNotification()->title(),
            u"Your administrator restarted your device");
}

TEST_F(RebootNotificationControllerTest, KioskSessionDoesNotShowNotification) {
  CreateFakeKioskUser();
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromUTCExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // User is not logged in. Don't show notifications.
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(nullptr, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(nullptr, GetPostRebootNotification());

  // Start kiosk session. Don't show notifications.
  LoginFakeUser();
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_EQ(nullptr, GetPendingRebootNotification());
  notification_controller_.MaybeShowPostRebootNotification();
  EXPECT_EQ(nullptr, GetPostRebootNotification());
}

TEST_F(RebootNotificationControllerTest, CloseNotification) {
  CreateFakeUser();
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // Log in user and show notification.
  LoginFakeUser();
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::NullCallback());
  EXPECT_NE(nullptr, GetPendingRebootNotification());
  EXPECT_EQ(GetNotificationCount(), 1);

  // Explicitly close notification.
  notification_controller_.CloseRebootNotification();
  EXPECT_EQ(nullptr, GetPendingRebootNotification());
  EXPECT_EQ(GetNotificationCount(), 0);
}

TEST_F(RebootNotificationControllerTest, HandleNotificationClick) {
  CreateFakeUser();
  base::Time reboot_time;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(kRebootTime2022Feb2At1520, &reboot_time));

  // Log in user and show notification.
  LoginFakeUser();
  ClickCounter counter;
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time, base::BindRepeating(&ClickCounter::ButtonClickCallback,
                                       counter.weak_ptr_factory_.GetWeakPtr()));
  const Notification* notification = GetPendingRebootNotification();
  ASSERT_TRUE(notification);
  // Click on notification and do nothing.
  notification->delegate()->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(counter.clicks, 0);
  // Click on notification button and run callback.
  notification->delegate()->Click(0, std::nullopt);
  EXPECT_EQ(counter.clicks, 1);
}
