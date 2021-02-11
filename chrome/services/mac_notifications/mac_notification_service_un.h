// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_

#include "base/mac/scoped_nsobject.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

@class AlertUNNotificationCenterDelegate;
@class UNUserNotificationCenter;

// Implementation of the MacNotificationService mojo interface using the
// UNNotification system API.
class API_AVAILABLE(macos(10.14)) MacNotificationServiceUN
    : public notifications::mojom::MacNotificationService {
 public:
  MacNotificationServiceUN(
      mojo::PendingReceiver<notifications::mojom::MacNotificationService>
          service,
      mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>
          handler,
      UNUserNotificationCenter* notification_center);
  MacNotificationServiceUN(const MacNotificationServiceUN&) = delete;
  MacNotificationServiceUN& operator=(const MacNotificationServiceUN&) = delete;
  ~MacNotificationServiceUN() override;

  // notifications::mojom::MacNotificationService:
  void CloseNotification(
      notifications::mojom::NotificationIdentifierPtr identifier) override;
  void CloseAllNotifications() override;

 private:
  // Requests notification permissions from the system. This will ask the user
  // to accept permissions if not granted or denied already.
  void RequestPermission();

  mojo::Receiver<notifications::mojom::MacNotificationService> binding_;
  base::scoped_nsobject<AlertUNNotificationCenterDelegate> delegate_;
  base::scoped_nsobject<UNUserNotificationCenter> notification_center_;
};

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UN_H_
