// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/internal/update_notification_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/notifications/scheduler/public/client_overview.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/schedule_service_utils.h"
#include "chrome/browser/notifications/scheduler/public/throttle_config.h"
#include "chrome/browser/updates/update_notification_config.h"  // nogncheck
#include "chrome/browser/updates/update_notification_info.h"    // nogncheck
#include "chrome/browser/updates/update_notification_service_bridge.h"  // nogncheck

namespace updates {

// Maximum number of update notification should be cached in scheduler.
constexpr int kNumMaxNotificationsLimit = 1;

// Maxmium number of consecutive dismiss actions from user that should be
// considered as negative feedback.
constexpr int kNumConsecutiveDismissCountCap = 2;

// String to represents the key of the map to store update state extra.
const char kUpdateStateEnumKey[] = "extra_data_map_key_update_state_enum";

namespace {

void BuildNotificationData(const updates::UpdateNotificationInfo& data,
                           notifications::NotificationData* out) {
  DCHECK(out);
  out->title = data.title;
  out->message = data.message;
  out->custom_data[kUpdateStateEnumKey] = base::NumberToString(data.state);
}

notifications::ScheduleParams BuildScheduleParams(
    bool should_show_immediately,
    base::Clock* clock,
    const UpdateNotificationConfig* config) {
  notifications::ScheduleParams schedule_params;
  schedule_params.impression_mapping.emplace(
      notifications::UserFeedback::kDismiss,
      notifications::ImpressionResult::kNegative);
  schedule_params.impression_mapping.emplace(
      notifications::UserFeedback::kNotHelpful,
      notifications::ImpressionResult::kNegative);
  if (should_show_immediately) {
    schedule_params.deliver_time_start = base::make_optional(clock->Now());
    schedule_params.deliver_time_end =
        base::make_optional(clock->Now() + base::TimeDelta::FromMinutes(1));
  } else {
    notifications::TimePair actual_window;
    notifications::NextTimeWindow(clock, config->deliver_window_morning,
                                  config->deliver_window_evening,
                                  &actual_window);
    schedule_params.deliver_time_start =
        base::make_optional(std::move(actual_window.first));
    schedule_params.deliver_time_end =
        base::make_optional(std::move(actual_window.second));
  }
  return schedule_params;
}

base::TimeDelta GetCurrentInterval(int num_suppresion,
                                   base::TimeDelta init_interval,
                                   base::TimeDelta max_interval) {
  return std::min(max_interval, (num_suppresion + 1) * init_interval);
}

bool TooManyNotificationCached(
    const notifications::ClientOverview& client_overview) {
  return client_overview.num_scheduled_notifications >=
         kNumMaxNotificationsLimit;
}

}  // namespace

UpdateNotificationServiceImpl::UpdateNotificationServiceImpl(
    notifications::NotificationScheduleService* schedule_service,
    std::unique_ptr<UpdateNotificationConfig> config,
    std::unique_ptr<UpdateNotificationServiceBridge> bridge,
    base::Clock* clock)
    : schedule_service_(schedule_service),
      config_(std::move(config)),
      bridge_(std::move(bridge)),
      clock_(clock) {}

UpdateNotificationServiceImpl::~UpdateNotificationServiceImpl() = default;

void UpdateNotificationServiceImpl::Schedule(UpdateNotificationInfo data) {
  schedule_service_->GetClientOverview(
      notifications::SchedulerClientType::kChromeUpdate,
      base::BindOnce(&UpdateNotificationServiceImpl::ScheduleInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(data)));
}

void UpdateNotificationServiceImpl::ScheduleInternal(
    UpdateNotificationInfo data,
    notifications::ClientOverview client_overview) {
  if (TooManyNotificationCached(client_overview))
    return;

  notifications::NotificationData notification_data;
  BuildNotificationData(data, &notification_data);
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kChromeUpdate,
      std::move(notification_data),
      BuildScheduleParams(data.should_show_immediately, clock_, config_.get()));
  params->enable_ihnr_buttons = true;
  schedule_service_->Schedule(std::move(params));
}

void UpdateNotificationServiceImpl::OnUserClick(const ExtraData& extra) {
  DCHECK(base::Contains(extra, kUpdateStateEnumKey));
  int state = 0;
  auto res = base::StringToInt(extra.at(kUpdateStateEnumKey), &state);
  DCHECK(res);
  bridge_->LaunchChromeActivity(state);
}

void UpdateNotificationServiceImpl::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  schedule_service_->GetClientOverview(
      notifications::SchedulerClientType::kChromeUpdate,
      base::BindOnce(&UpdateNotificationServiceImpl::DetermineThrottleConfig,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UpdateNotificationServiceImpl::DetermineThrottleConfig(
    ThrottleConfigCallback callback,
    notifications::ClientOverview client_overview) {
  auto throttle_config = std::make_unique<notifications::ThrottleConfig>();
  // 2 consecutive dismiss will cause a suppression.
  throttle_config->negative_action_count_threshold =
      kNumConsecutiveDismissCountCap;
  auto num_suppresion = client_overview.impression_detail.num_negative_events;
  // Next suppression duration is proportional to the number of suppression
  // events, ceiled with a maximum duration interval.
  throttle_config->suppression_duration = GetCurrentInterval(
      num_suppresion, config_->init_interval, config_->max_interval);
  std::move(callback).Run(std::move(throttle_config));
}

void UpdateNotificationServiceImpl::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {
  schedule_service_->GetClientOverview(
      notifications::SchedulerClientType::kChromeUpdate,
      base::BindOnce(&UpdateNotificationServiceImpl::MaybeShowNotification,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(notification_data), std::move(callback)));
}

bool UpdateNotificationServiceImpl::TooSoonForNextNotification(
    const notifications::ClientOverview& client_overview) {
  auto last_shown_timestamp = client_overview.impression_detail.last_shown_ts;
  auto next_notificaiton_interval =
      GetCurrentInterval(client_overview.impression_detail.num_negative_events,
                         config_->init_interval, config_->max_interval);
  return last_shown_timestamp.has_value() &&
         next_notificaiton_interval >=
             clock_->Now() - last_shown_timestamp.value();
}

void UpdateNotificationServiceImpl::MaybeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback,
    notifications::ClientOverview client_overview) {
  bool should_show_notification =
      config_->is_enabled && !TooSoonForNextNotification(client_overview) &&
      !TooManyNotificationCached(client_overview);

  std::move(callback).Run(
      should_show_notification ? std::move(notification_data) : nullptr);
}

}  // namespace updates
