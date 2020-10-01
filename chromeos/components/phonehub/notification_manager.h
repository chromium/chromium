// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_H_

#include <stdint.h>

#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/notification.h"

namespace chromeos {
namespace phonehub {

class Notification;

// Tracks notifications which have been synced from a connected phone during a
// Phone Hub session. Clients can access notifications via GetNotification() and
// can be notified when the state of notifications changes by registering as
// observers.
//
// This class also provides functionality for interacting with notifications;
// namely, clients can dismiss notifications or send inline replies.
class NotificationManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnNotificationsAdded(
        const base::flat_set<int64_t>& notification_ids) {}
    virtual void OnNotificationsUpdated(
        const base::flat_set<int64_t>& notification_ids) {}
    virtual void OnNotificationsRemoved(
        const base::flat_set<int64_t>& notification_ids) {}
  };

  NotificationManager(const NotificationManager&) = delete;
  NotificationManager& operator=(const NotificationManager&) = delete;
  virtual ~NotificationManager();

  // Returns null if no notification exists with the given ID. Pointers returned
  // by this function should not be cached, since the underlying Notification
  // object may be deleted by a future update.
  virtual const Notification* GetNotification(
      int64_t notification_id) const = 0;

  // Dismisses the notification with the given ID; if no notification exists
  // with this ID, this function is a no-op.
  virtual void DismissNotification(int64_t notification_id) = 0;

  // Sends an inline reply for the notificaiton with the given ID; if no
  // notification exists with this ID, this function is a no-op.
  virtual void SendInlineReply(int64_t notification_id,
                               const base::string16& inline_reply_text) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class PhoneStatusProcessor;

  NotificationManager();

  // Sets the internal collection of notifications. This does not send any
  // requests to the remote phone device.
  virtual void SetNotificationsInternal(
      const base::flat_set<Notification>& notifications) = 0;

  // Removes the dismissed notifications from the internal collection of
  // notifications. Does not send a request to remove notifications to the
  // remote device.
  virtual void RemoveNotificationsInternal(
      const base::flat_set<int64_t>& notification_ids) = 0;

  // Clears the underlying internal collection of notifications. This does not
  // send any requests to clear the phone's notifications.
  virtual void ClearNotificationsInternal() = 0;

  void NotifyNotificationsAdded(
      const base::flat_set<int64_t>& notification_ids);
  void NotifyNotificationsUpdated(
      const base::flat_set<int64_t>& notification_ids);
  void NotifyNotificationsRemoved(
      const base::flat_set<int64_t>& notification_ids);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_H_
