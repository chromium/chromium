// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_NS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_NS_H_

#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

@class AlertNSNotificationCenterDelegate;
@class NSUserNotificationCenter;

namespace mac_notifications {

// Implementation of the MacNotificationService mojo interface using the
// NSUserNotification system API.
class MacNotificationServiceNS : public mojom::MacNotificationService {
 public:
  MacNotificationServiceNS(
      mojo::PendingReceiver<mojom::MacNotificationService> service,
      mojo::PendingRemote<mojom::MacNotificationActionHandler> handler,
      NSUserNotificationCenter* notification_center);
  MacNotificationServiceNS(const MacNotificationServiceNS&) = delete;
  MacNotificationServiceNS& operator=(const MacNotificationServiceNS&) = delete;
  ~MacNotificationServiceNS() override;

  // mojom::MacNotificationService:
  void DisplayNotification(mojom::NotificationPtr notification) override;
  void GetDisplayedNotifications(
      mojom::ProfileIdentifierPtr profile,
      GetDisplayedNotificationsCallback callback) override;
  void CloseNotification(mojom::NotificationIdentifierPtr identifier) override;
  void CloseNotificationsForProfile(
      mojom::ProfileIdentifierPtr profile) override;
  void CloseAllNotifications() override;

 private:
  mojo::Receiver<mojom::MacNotificationService> binding_;
  AlertNSNotificationCenterDelegate* __strong delegate_;
  NSUserNotificationCenter* __strong notification_center_;
};

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_NS_H_
