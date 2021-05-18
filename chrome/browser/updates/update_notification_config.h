// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CONFIG_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CONFIG_H_

#include <memory>
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

// Configure the initial schedule interval update notification in days.
constexpr char kUpdateNotificationInitIntervalParamName[] =
    "update_notification_init_interval_days";

// Configure the maximum schedule interval update notification in days.
constexpr char kUpdateNotificationMaxIntervalParamName[] =
    "update_notification_max_interval_days";

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

struct UpdateNotificationConfig {
  // Create a default update notification config.
  static std::unique_ptr<UpdateNotificationConfig> Create();

  // Create an update notification config read from Finch.
  static std::unique_ptr<UpdateNotificationConfig> CreateFromFinch();

  UpdateNotificationConfig();
  ~UpdateNotificationConfig();

  // Flag to tell whether update notification is enabled or not.
  bool is_enabled;

  // Default interval to schedule next update notification.
  base::TimeDelta init_interval;

  // Maximum interval to schedule next update notification.
  base::TimeDelta max_interval;

  // Deliver window pair [start, end] in the morning.
  std::pair<base::TimeDelta, base::TimeDelta> deliver_window_morning;

  // Deliver window pair [start, end] in the evening.
  std::pair<base::TimeDelta, base::TimeDelta> deliver_window_evening;
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_CONFIG_H_
