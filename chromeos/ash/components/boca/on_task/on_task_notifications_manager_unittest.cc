// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotifierId;
using ::testing::IsNull;
using ::testing::NotNull;

namespace ash::boca {
namespace {

constexpr char kTestNotificationId[] = "TestOnTaskNotification";
constexpr std::u16string_view kTestNotificationTitle =
    u"TestOnTaskNotificationTitle";
constexpr std::u16string_view kTestNotificationMessage =
    u"TestOnTaskNotificationMessage";
constexpr std::u16string_view kToastDescription = u"TestDescription";
constexpr base::TimeDelta kToastCountdownPeriod = base::Seconds(5);

// Fake delegate implementation for the `OnTaskNotificationsManager`.
class FakeOnTaskNotificationsManagerDelegate
    : public OnTaskNotificationsManager::Delegate {
 public:
  FakeOnTaskNotificationsManagerDelegate() = default;
  ~FakeOnTaskNotificationsManagerDelegate() override = default;

  // OnTaskNotificationsManager::Delegate:
  void ShowToast(ToastData toast_data) override { ++toast_count_; }
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    ++notification_count_;
  }
  void ClearNotification(const std::string& id) override {
    if (GetNotificationCount() > 0u) {
      --notification_count_;
    }
  }

  size_t GetToastCount() { return toast_count_.load(); }
  size_t GetNotificationCount() { return notification_count_.load(); }

 private:
  std::atomic<size_t> toast_count_;
  std::atomic<size_t> notification_count_;
};

class OnTaskNotificationsManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto fake_delegate =
        std::make_unique<FakeOnTaskNotificationsManagerDelegate>();
    fake_delegate_ptr_ = fake_delegate.get();
    notifications_manager_ =
        OnTaskNotificationsManager::CreateForTest(std::move(fake_delegate));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<OnTaskNotificationsManager> notifications_manager_;
  raw_ptr<FakeOnTaskNotificationsManagerDelegate> fake_delegate_ptr_;
};

TEST_F(OnTaskNotificationsManagerTest, CreateToastWithNoCountdownPeriod) {
  bool callback_triggered = false;
  OnTaskNotificationsManager::ToastCreateParams create_params(
      /*id=*/kTestNotificationId,
      /*catalog_name=*/ToastCatalogName::kMaxValue,
      /*text_description_callback=*/
      base::BindLambdaForTesting([](base::TimeDelta countdown_period) {
        return std::u16string(kToastDescription);
      }),
      /*completion_callback=*/base::BindLambdaForTesting([&]() {
        callback_triggered = true;
      }));
  notifications_manager_->CreateToast(std::move(create_params));

  // Verify toast is shown after a 1 second delay because of the scheduled
  // timer.
  task_environment_.FastForwardBy(kOnTaskNotificationCountdownInterval);
  ASSERT_EQ(fake_delegate_ptr_->GetToastCount(), 1u);
  ASSERT_FALSE(callback_triggered);

  // Advance timer by 1 more second and verify callback is triggered.
  task_environment_.FastForwardBy(kOnTaskNotificationCountdownInterval);
  EXPECT_TRUE(callback_triggered);
}

TEST_F(OnTaskNotificationsManagerTest,
       NewToastsWithSameIdOverridePreviousOnes) {
  bool callback_triggered_1 = false;
  OnTaskNotificationsManager::ToastCreateParams create_params_1(
      /*id=*/kTestNotificationId,
      /*catalog_name=*/ToastCatalogName::kMaxValue,
      /*text_description_callback=*/
      base::BindLambdaForTesting([](base::TimeDelta countdown_period) {
        return std::u16string(kToastDescription);
      }),
      /*completion_callback=*/base::BindLambdaForTesting([&]() {
        callback_triggered_1 = true;
      }),
      /*countdown_period=*/kToastCountdownPeriod);
  notifications_manager_->CreateToast(std::move(create_params_1));

  // Verify toast is shown after a 1 second delay.
  task_environment_.FastForwardBy(kOnTaskNotificationCountdownInterval);
  ASSERT_EQ(fake_delegate_ptr_->GetToastCount(), 1u);

  // Trigger another toast with the same id.
  bool callback_triggered_2 = false;
  OnTaskNotificationsManager::ToastCreateParams create_params_2(
      /*id=*/kTestNotificationId,
      /*catalog_name=*/ToastCatalogName::kMaxValue,
      /*text_description_callback=*/
      base::BindLambdaForTesting([](base::TimeDelta countdown_period) {
        return std::u16string(kToastDescription);
      }),
      /*completion_callback=*/base::BindLambdaForTesting([&]() {
        callback_triggered_2 = true;
      }),
      /*countdown_period=*/kToastCountdownPeriod);
  notifications_manager_->CreateToast(std::move(create_params_2));

  // Advance timer and verify the first toast is overridden by ensuring the
  // corresponding callback is not triggered.
  task_environment_.FastForwardBy(kToastCountdownPeriod +
                                  kToastCountdownPeriod);
  EXPECT_FALSE(callback_triggered_1);
  EXPECT_TRUE(callback_triggered_2);
}

TEST_F(OnTaskNotificationsManagerTest, StopProcessingToast) {
  OnTaskNotificationsManager::ToastCreateParams create_params(
      /*id=*/kTestNotificationId,
      /*catalog_name=*/ToastCatalogName::kMaxValue,
      /*text_description_callback=*/
      base::BindLambdaForTesting([](base::TimeDelta countdown_period) {
        return std::u16string(kToastDescription);
      }),
      /*completion_callback=*/base::DoNothing(),
      /*countdown_period=*/kToastCountdownPeriod);
  notifications_manager_->CreateToast(std::move(create_params));

  // Verify toast is shown after a 1 second delay.
  task_environment_.FastForwardBy(kOnTaskNotificationCountdownInterval);
  ASSERT_EQ(fake_delegate_ptr_->GetToastCount(), 1u);

  // Attempt to stop processing notification and verify toast is not shown with
  // subsequent timer advances.
  notifications_manager_->StopProcessingNotification(kTestNotificationId);
  task_environment_.FastForwardBy(kOnTaskNotificationCountdownInterval);
  EXPECT_EQ(fake_delegate_ptr_->GetToastCount(), 1u);
}

TEST_F(OnTaskNotificationsManagerTest,
       TriggerCompletionCallbackOnToastCountdownEnd) {
  bool callback_triggered = false;
  OnTaskNotificationsManager::ToastCreateParams create_params(
      /*id=*/kTestNotificationId,
      /*catalog_name=*/ToastCatalogName::kMaxValue,
      /*text_description_callback=*/
      base::BindLambdaForTesting([](base::TimeDelta countdown_period) {
        return std::u16string(kToastDescription);
      }),
      /*completion_callback=*/
      base::BindLambdaForTesting([&]() { callback_triggered = true; }),
      /*countdown_period=*/kToastCountdownPeriod);
  notifications_manager_->CreateToast(std::move(create_params));

  // Verify toast is shown after a 1 second delay.
  task_environment_.FastForwardBy(kOnTaskNotificationCountdownInterval);
  ASSERT_EQ(fake_delegate_ptr_->GetToastCount(), 1u);
  ASSERT_FALSE(callback_triggered);

  // Toasts remain visible before count down.
  task_environment_.FastForwardBy(kOnTaskNotificationCountdownInterval);
  ASSERT_EQ(fake_delegate_ptr_->GetToastCount(), 2u);
  ASSERT_FALSE(callback_triggered);

  // Verify callback is triggered after the countdown period.
  task_environment_.FastForwardBy(kToastCountdownPeriod);
  EXPECT_TRUE(callback_triggered);
}

TEST_F(OnTaskNotificationsManagerTest, CreateNotification) {
  OnTaskNotificationsManager::NotificationCreateParams create_params(
      /*id=*/kTestNotificationId,
      /*title=*/std::u16string{kTestNotificationTitle},
      /*message=*/std::u16string{kTestNotificationMessage},
      /*notifier_id=*/NotifierId());
  notifications_manager_->CreateNotification(std::move(create_params));
  EXPECT_EQ(fake_delegate_ptr_->GetNotificationCount(), 1u);
}

TEST_F(OnTaskNotificationsManagerTest, ConfigureForLockedWindow) {
  notifications_manager_->ConfigureForLockedMode(/*locked=*/true);
  EXPECT_THAT(notifications_manager_->GetNotificationBlockerForTesting(),
              NotNull());
}

TEST_F(OnTaskNotificationsManagerTest, ConfigureForUnlockedWindow) {
  notifications_manager_->ConfigureForLockedMode(/*locked=*/false);
  EXPECT_THAT(notifications_manager_->GetNotificationBlockerForTesting(),
              IsNull());
}

}  // namespace
}  // namespace ash::boca
