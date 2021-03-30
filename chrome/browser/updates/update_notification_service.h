// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"  // nognservice

namespace notifications {
struct ThrottleConfig;
struct NotificationData;
}  // namespace notifications

namespace updates {

struct UpdateNotificationInfo;

// Service to schedule update notification via
// notifications::NotificationScheduleService.
class UpdateNotificationService : public KeyedService {
 public:
  using ExtraData = std::map<std::string, std::string>;
  using ThrottleConfigCallback =
      base::OnceCallback<void(std::unique_ptr<notifications::ThrottleConfig>)>;
  using NotificationDataCallback = base::OnceCallback<void(
      std::unique_ptr<notifications::NotificationData>)>;

  // Schedule an update notification.
  virtual void Schedule(UpdateNotificationInfo data) = 0;

  // Called when the notification is clicked by user. Passing |extra| for
  // processing custom data.
  virtual void OnUserClick(const ExtraData& extra) = 0;

  // Replies customized throttle config.
  virtual void GetThrottleConfig(ThrottleConfigCallback callback) = 0;

  // Confirm whether the upcoming notification is ready to display.
  virtual void BeforeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback) = 0;

  ~UpdateNotificationService() override = default;

 protected:
  UpdateNotificationService() = default;
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_H_
