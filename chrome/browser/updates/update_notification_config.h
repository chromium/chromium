// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CONFIG_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CONFIG_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/time/time.h"

namespace updates {

// Some of these are also existing in Java side, please refer:
// src/chrome/android/java/src/org/
// chromium/chrome/browser/omaha/UpdateConfigs.java

// Configure the update notification enable/disable switch.
constexpr char kUpdateNotificationStateParamName[] =
    "update_notification_state";

// Configure the update notification schedule interval in days.
constexpr char kUpdateNotificationIntervalParamName[] =
    "update_notification_interval_days";

// Configure the start of deliver window in the morning.
constexpr char kUpdateNotificationDeliverWindowMorningStartParamName[] =
    "update_notification_deliver_window_morning_start";

// Configure the end of deliver window in the morning.
constexpr char kUpdateNotificationDeliverWindowMorningEndParamName[] =
    "update_notification_deliver_window_morning_end";

// Configure the start of deliver window in the evening.
constexpr char kUpdateNotificationDeliverWindowEveningStartParamName[] =
    "update_notification_deliver_window_evening_start";

// Configure the end of deliver window in the evening.
constexpr char kUpdateNotificationDeliverWindowEveningEndParamName[] =
    "update_notification_deliver_window_evening_end";

// Configure the custom throttle linear function scale coefficient.
constexpr char kUpdateNotificationDeliverScaleCoefficientParamName[] =
    "update_notification_throttle_co_scale";

// Configure the custom throttle linear function offset coefficient.
constexpr char kUpdateNotificationThrottleOffsetCoefficientParamName[] =
    "update_notification_throttle_co_offset";

struct UpdateNotificationConfig {
  // Create a update notification config.
  static std::unique_ptr<UpdateNotificationConfig> Create();

  UpdateNotificationConfig();
  ~UpdateNotificationConfig();

  // Flag to tell whether update notification is enabled or not.
  bool is_enabled;

  // Default interval to schedule next update notification.
  base::TimeDelta default_interval;

  // Throttle logic could be conducted by adjusting schedule interval.
  // A linear function X' = a * X + b would be applied.
  // X represents the current interval(init value is default_interval),
  // a represents scale coefficient,
  // b represents offset coefficient,
  // X' represents new schedule interval after applying throttling logic.
  // For example, default_interval X is 3 weeks(21 days), a = 2, b = 2,
  // then the schedule interval X' would be 8 weeks if it is throttled,
  // and next time should be 18 weeks etc.

  // Scale coefficient a in the linear function X' = a * X + b;
  double throttle_interval_linear_co_scale;

  // Offset coefficient b in the linear function X' = a * X + b;
  double throttle_interval_linear_co_offset;

  // Deliver window pair [start, end] in the morning.
  std::pair<base::TimeDelta, base::TimeDelta> deliver_window_morning;

  // Deliver window pair [start, end] in the evening.
  std::pair<base::TimeDelta, base::TimeDelta> deliver_window_evening;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationConfig);
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CONFIG_H_
