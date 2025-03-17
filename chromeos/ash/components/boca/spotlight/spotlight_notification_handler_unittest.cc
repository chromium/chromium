// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_notification_handler.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_notification_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash::boca {
namespace {

class FakeSpotlightNotificationHandlerDelegate
    : public SpotlightNotificationHandler::Delegate {
 public:
  FakeSpotlightNotificationHandlerDelegate() = default;
  ~FakeSpotlightNotificationHandlerDelegate() override = default;

  // SpotlightNotificationHandler::Delegate
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    called_show_count_++;
  }
  void ClearNotification(const std::string& id) override { cancel_called_++; }

  int called_show_count() { return called_show_count_; }
  int cancel_called_count() { return cancel_called_; }

 private:
  int called_show_count_;
  int cancel_called_;
};

class SpotlightNotificationHandlerTest : public testing::Test {
 public:
  SpotlightNotificationHandlerTest() = default;

  void SetUp() override {
    std::unique_ptr<FakeSpotlightNotificationHandlerDelegate> delegate =
        std::make_unique<FakeSpotlightNotificationHandlerDelegate>();
    delegate_ptr_ = delegate.get();
    handler_ =
        std::make_unique<SpotlightNotificationHandler>(std::move(delegate));
  }

  void TearDown() override {
    delegate_ptr_ = nullptr;
    handler_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<FakeSpotlightNotificationHandlerDelegate> delegate_ptr_;
  std::unique_ptr<SpotlightNotificationHandler> handler_;
};

TEST_F(SpotlightNotificationHandlerTest, StartSpotlightCountdownNotification) {
  bool callback_triggered = false;
  handler_->StartSpotlightCountdownNotification(
      base::BindLambdaForTesting([&]() { callback_triggered = true; }));
  task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);

  int notification_called_count = 1;
  for (base::TimeDelta i = kSpotlightNotificationDuration; i.is_positive();
       i = i - kSpotlightNotificationCountdownInterval) {
    std::u16string expected_string =
        l10n_util::GetStringFUTF16(IDS_BOCA_SPOTLIGHT_NOTIFICATION_MESSAGE,
                                   base::NumberToString16(i.InSeconds()));

    ASSERT_EQ(delegate_ptr_->called_show_count(), notification_called_count);
    ASSERT_FALSE(callback_triggered);
    notification_called_count++;
    task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);
  }

  EXPECT_TRUE(callback_triggered);
}

TEST_F(SpotlightNotificationHandlerTest,
       StartSpotlightCountdownNotificationOverridesExistingRequest) {
  bool callback_1_triggered = false;
  bool callback_2_triggered = false;
  handler_->StartSpotlightCountdownNotification(
      base::BindLambdaForTesting([&]() { callback_1_triggered = true; }));

  task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);

  // Send second request while first is in progress.
  handler_->StartSpotlightCountdownNotification(
      base::BindLambdaForTesting([&]() { callback_2_triggered = true; }));

  task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);

  for (base::TimeDelta i = kSpotlightNotificationDuration; i.is_positive();
       i = i - kSpotlightNotificationCountdownInterval) {
    task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);
  }
  EXPECT_FALSE(callback_1_triggered);
  EXPECT_TRUE(callback_2_triggered);
}

TEST_F(SpotlightNotificationHandlerTest, StopSpotlightNotification) {
  bool callback_triggered = false;
  handler_->StartSpotlightCountdownNotification(
      base::BindLambdaForTesting([&]() { callback_triggered = true; }));
  task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);
  ASSERT_EQ(delegate_ptr_->called_show_count(), 1);

  handler_->StopSpotlightCountdown();
  for (base::TimeDelta i = kSpotlightNotificationDuration; i.is_positive();
       i = i - kSpotlightNotificationCountdownInterval) {
    task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);
  }

  EXPECT_FALSE(callback_triggered);
  EXPECT_EQ(delegate_ptr_->called_show_count(), 1);
  EXPECT_EQ(delegate_ptr_->cancel_called_count(), 1);
}
}  // namespace
}  // namespace ash::boca
