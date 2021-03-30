// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_INTERNAL_UPDATE_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_UPDATES_INTERNAL_UPDATE_NOTIFICATION_SERVICE_IMPL_H_

#include "chrome/browser/updates/update_notification_service.h"  // nogncheck

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"

namespace notifications {
struct ClientOverview;
class NotificationScheduleService;
}  // namespace notifications

namespace updates {

struct UpdateNotificationConfig;
struct UpdateNotificationInfo;
class UpdateNotificationServiceBridge;

class UpdateNotificationServiceImpl : public UpdateNotificationService {
 public:
  UpdateNotificationServiceImpl(
      notifications::NotificationScheduleService* schedule_service,
      std::unique_ptr<UpdateNotificationConfig> config,
      std::unique_ptr<UpdateNotificationServiceBridge> bridge,
      base::Clock* clock);
  ~UpdateNotificationServiceImpl() override;

 private:
  // UpdateNotificationService implementation.
  void Schedule(UpdateNotificationInfo data) override;
  void OnUserClick(const ExtraData& extra) override;
  void GetThrottleConfig(ThrottleConfigCallback callback) override;
  void BeforeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback) override;

  // Calculates the params of throttle config depends on |client_overview|, and
  // reply to the |callback|. suppresion duration length starts with
  // |init_interval| read from config, and the increase is proportional to
  // the number of suppression events, until reach the |max_interva|l in config.
  void DetermineThrottleConfig(ThrottleConfigCallback callback,
                               notifications::ClientOverview client_overview);

  // Called before displaying the notification, and will reply nullptr to
  // callback if it's not the right time to show this upcoming notification.
  void MaybeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback,
      notifications::ClientOverview client_overview);

  // Return true if |current timestamp - last notification shown timestamp| is
  // smailler than interval, which is based on the config and number of throttle
  // events read from |client_overview|.
  bool TooSoonForNextNotification(
      const notifications::ClientOverview& client_overview);

  // Called before using notification schedule service to actually schedule.
  // Will not schedule if already has too many notifications cached.
  void ScheduleInternal(UpdateNotificationInfo data,
                        notifications::ClientOverview client_overview);

  // Used to schedule notification to show in the future. Must outlive this
  // class.
  notifications::NotificationScheduleService* schedule_service_;

  std::unique_ptr<UpdateNotificationConfig> config_;

  std::unique_ptr<UpdateNotificationServiceBridge> bridge_;

  base::Clock* clock_;

  base::WeakPtrFactory<UpdateNotificationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_INTERNAL_UPDATE_NOTIFICATION_SERVICE_IMPL_H_
