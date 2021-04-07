// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/internal/update_notification_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/public/schedule_service_utils.h"
#include "chrome/browser/notifications/scheduler/public/throttle_config.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "chrome/browser/updates/test/mock_update_notification_service_bridge.h"
#include "chrome/browser/updates/update_notification_config.h"
#include "chrome/browser/updates/update_notification_info.h"

namespace updates {
namespace {

using testing::_;
using ::testing::Invoke;

const auto kTestTitle = base::UTF8ToUTF16("hello");
const auto kTestMessage = base::UTF8ToUTF16("world");

class UpdateNotificationServiceImplTest : public testing::Test {
 public:
  UpdateNotificationServiceImplTest() : bridge_(nullptr), config_(nullptr) {}
  ~UpdateNotificationServiceImplTest() override = default;
  UpdateNotificationServiceImplTest(
      const UpdateNotificationServiceImplTest& other) = delete;
  UpdateNotificationServiceImplTest& operator=(
      const UpdateNotificationServiceImplTest& other) = delete;

  void SetUp() override {
    scheduler_ = std::make_unique<
        notifications::test::MockNotificationScheduleService>();
    auto bridge = std::make_unique<test::MockUpdateNotificationServiceBridge>();
    bridge_ = bridge.get();
    auto config = UpdateNotificationConfig::Create();
    config_ = config.get();
    config_->is_enabled = true;
    service_ = std::make_unique<updates::UpdateNotificationServiceImpl>(
        scheduler_.get(), std::move(config), std::move(bridge), &clock_);
  }

 protected:
  notifications::test::MockNotificationScheduleService* scheduler() {
    return scheduler_.get();
  }
  test::MockUpdateNotificationServiceBridge* bridge() { return bridge_; }
  UpdateNotificationService* service() { return service_.get(); }
  UpdateNotificationConfig* config() { return config_; }
  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  test::MockUpdateNotificationServiceBridge* bridge_;
  std::unique_ptr<notifications::test::MockNotificationScheduleService>
      scheduler_;
  UpdateNotificationConfig* config_;

  std::unique_ptr<UpdateNotificationService> service_;
};

MATCHER_P(NotificationParamsEq,
          expected,
          "Compare the notification params except GUID") {
  EXPECT_EQ(arg->schedule_params, expected->schedule_params);
  EXPECT_EQ(arg->notification_data, expected->notification_data);
  EXPECT_EQ(arg->enable_ihnr_buttons, expected->enable_ihnr_buttons);
  EXPECT_EQ(arg->type, expected->type);
  return true;
}

TEST_F(UpdateNotificationServiceImplTest, Schedule) {
  base::Time fake_now;
  EXPECT_TRUE(base::Time::FromString("05/18/20 01:00:00 AM", &fake_now));
  clock()->SetNow(fake_now);

  notifications::ScheduleParams expected_schedule_params;
  expected_schedule_params.impression_mapping.emplace(
      notifications::UserFeedback::kDismiss,
      notifications::ImpressionResult::kNegative);
  expected_schedule_params.impression_mapping.emplace(
      notifications::UserFeedback::kNotHelpful,
      notifications::ImpressionResult::kNegative);
  notifications::TimePair deliver_window;
  notifications::NextTimeWindow(clock(), config()->deliver_window_morning,
                                config()->deliver_window_evening,
                                &deliver_window);
  expected_schedule_params.deliver_time_start =
      base::make_optional(std::move(deliver_window.first));
  expected_schedule_params.deliver_time_end =
      base::make_optional(std::move(deliver_window.second));

  notifications::NotificationData expected_notification_data;
  expected_notification_data.title = kTestTitle;
  expected_notification_data.message = kTestMessage;
  expected_notification_data
      .custom_data["extra_data_map_key_update_state_enum"] = "1";

  notifications::NotificationParams expected_params(
      notifications::SchedulerClientType::kChromeUpdate,
      std::move(expected_notification_data),
      std::move(expected_schedule_params));
  expected_params.enable_ihnr_buttons = true;

  EXPECT_CALL(
      *scheduler(),
      GetClientOverview(notifications::SchedulerClientType::kChromeUpdate, _))
      .WillOnce(Invoke(
          [](notifications::SchedulerClientType client,
             base::OnceCallback<void(notifications::ClientOverview)> callback) {
            notifications::ClientOverview client_overview;
            client_overview.num_scheduled_notifications = 0;
            std::move(callback).Run(std::move(client_overview));
          }));
  EXPECT_CALL(*scheduler(), Schedule(NotificationParamsEq(&expected_params)));

  UpdateNotificationInfo data;
  data.title = kTestTitle;
  data.message = kTestMessage;
  data.state = 1;
  data.should_show_immediately = false;
  service()->Schedule(std::move(data));
}

TEST_F(UpdateNotificationServiceImplTest, VerifyOnUserClick) {
  std::vector<int> update_states = {1 /*UPDATE_AVAILABLE*/,
                                    3 /*INLINE_UPDATE_AVAILABLE*/};
  for (const auto& state : update_states) {
    UpdateNotificationService::ExtraData extra = {
        {"extra_data_map_key_update_state_enum", base::NumberToString(state)}};
    EXPECT_CALL(*bridge(), LaunchChromeActivity(state));
    service()->OnUserClick(extra);
  }
}

TEST_F(UpdateNotificationServiceImplTest, VerifyGetThrottleConfig) {
  for (int i = 0; i < 4; i++) {
    EXPECT_CALL(
        *scheduler(),
        GetClientOverview(notifications::SchedulerClientType::kChromeUpdate, _))
        .WillOnce(
            Invoke([&i](notifications::SchedulerClientType client,
                        base::OnceCallback<void(notifications::ClientOverview)>
                            callback) {
              notifications::ClientOverview client_overview;
              client_overview.impression_detail.num_negative_events = i;
              std::move(callback).Run(std::move(client_overview));
            }));
    service()->GetThrottleConfig(base::BindOnce(
        [](int num_suppression,
           std::unique_ptr<notifications::ThrottleConfig> throttle_config) {
          EXPECT_EQ(throttle_config->suppression_duration.value(),
                    base::TimeDelta::FromDays(
                        std::min(21 * (1 + num_suppression), 90)));
          EXPECT_EQ(throttle_config->negative_action_count_threshold.value(),
                    2);
        },
        i));
  }
}

TEST_F(UpdateNotificationServiceImplTest, BeforeShowNotification) {
  std::vector<int> last_shown_timestamp_test_cases = {0, 20, 21, 22};
  for (int test_case : last_shown_timestamp_test_cases) {
    EXPECT_CALL(
        *scheduler(),
        GetClientOverview(notifications::SchedulerClientType::kChromeUpdate, _))
        .WillOnce(
            Invoke([&](notifications::SchedulerClientType client,
                       base::OnceCallback<void(notifications::ClientOverview)>
                           callback) {
              base::Time fake_now;
              EXPECT_TRUE(
                  base::Time::FromString("05/18/20 01:00:00 AM", &fake_now));
              clock()->SetNow(fake_now);
              notifications::ClientOverview client_overview;
              client_overview.impression_detail.last_shown_ts =
                  this->clock()->Now() - base::TimeDelta::FromDays(test_case);
              std::move(callback).Run(std::move(client_overview));
            }));
    service()->BeforeShowNotification(
        std::make_unique<notifications::NotificationData>(),
        base::BindOnce(
            [](bool should_show,
               std::unique_ptr<notifications::NotificationData> data) {
              if (should_show)
                EXPECT_NE(data, nullptr);
              else
                EXPECT_EQ(data, nullptr);
            },
            test_case > 21));
  }
}

}  // namespace
}  // namespace updates
