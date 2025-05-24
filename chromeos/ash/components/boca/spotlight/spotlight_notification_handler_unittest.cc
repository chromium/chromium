// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_notification_handler.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_notification_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

using base::test::TestFuture;

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
  TestFuture<void> countdown_completion_callback;
  handler_->StartSpotlightCountdownNotification(
      countdown_completion_callback.GetCallback());
  task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);

  int notification_called_count = 1;
  for (base::TimeDelta i = kSpotlightNotificationDuration; i.is_positive();
       i = i - kSpotlightNotificationCountdownInterval) {
    std::u16string expected_string =
        l10n_util::GetStringFUTF16(IDS_BOCA_SPOTLIGHT_NOTIFICATION_MESSAGE,
                                   base::NumberToString16(i.InSeconds()));

    EXPECT_EQ(delegate_ptr_->called_show_count(), notification_called_count);
    notification_called_count++;
    task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);
  }
  ASSERT_TRUE(countdown_completion_callback.Wait());
}

TEST_F(SpotlightNotificationHandlerTest, StopSpotlightNotification) {
  handler_->StartSpotlightCountdownNotification(base::BindOnce(
      []() { GTEST_FAIL() << "Unexpected call to completion callback"; }));
  task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);
  ASSERT_EQ(delegate_ptr_->called_show_count(), 1);

  handler_->StopSpotlightCountdown();
  for (base::TimeDelta i = kSpotlightNotificationDuration; i.is_positive();
       i = i - kSpotlightNotificationCountdownInterval) {
    task_environment_.FastForwardBy(kSpotlightNotificationCountdownInterval);
  }
  EXPECT_EQ(delegate_ptr_->called_show_count(), 1);
  EXPECT_EQ(delegate_ptr_->cancel_called_count(), 1);
}
}  // namespace
}  // namespace ash::boca
