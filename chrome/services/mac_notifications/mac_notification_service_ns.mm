// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_service_ns.h"

#import <Foundation/Foundation.h>
#import <Foundation/NSUserNotification.h>

#include <utility>

#include "mojo/public/cpp/bindings/remote.h"

@interface AlertNSNotificationCenterDelegate
    : NSObject <NSUserNotificationCenterDelegate>
- (instancetype)initWithActionHandler:
    (mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>)
        handler;
@end

MacNotificationServiceNS::MacNotificationServiceNS(
    mojo::PendingReceiver<notifications::mojom::MacNotificationService> service,
    mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>
        handler,
    NSUserNotificationCenter* notification_center)
    : binding_(this, std::move(service)),
      delegate_([[AlertNSNotificationCenterDelegate alloc]
          initWithActionHandler:std::move(handler)]),
      notification_center_([notification_center retain]) {
  [notification_center_ setDelegate:delegate_.get()];
}

MacNotificationServiceNS::~MacNotificationServiceNS() {
  [notification_center_ setDelegate:nil];
}

void MacNotificationServiceNS::CloseAllNotifications() {
  [notification_center_ removeAllDeliveredNotifications];
}

@implementation AlertNSNotificationCenterDelegate {
  mojo::Remote<notifications::mojom::MacNotificationActionHandler> _handler;
}

- (instancetype)initWithActionHandler:
    (mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>)
        handler {
  if ((self = [super init])) {
    _handler.Bind(std::move(handler));
  }
  return self;
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  auto actionInfo = notifications::mojom::NotificationActionInfo::New();
  // TODO(knollr): Fill |action_info| with details from |notification|.
  _handler->OnNotificationAction(std::move(actionInfo));
}

// Overriden from _NSUserNotificationCenterDelegatePrivate.
// Emitted when a user clicks the "Close" button in the notification.
// It not is emitted if the notification is closed from the notification
// center or if the app is not running at the time the Close button is
// pressed so it's essentially just a best effort way to detect
// notifications closed by the user.
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
               didDismissAlert:(NSUserNotification*)notification {
  auto actionInfo = notifications::mojom::NotificationActionInfo::New();
  // TODO(knollr): Fill |action_info| with details from |notification|.
  _handler->OnNotificationAction(std::move(actionInfo));
}

// Overriden from _NSUserNotificationCenterDelegatePrivate.
// Emitted when a user closes a notification from the notification center.
// This is an undocumented method introduced in 10.8 according to
// https://bugzilla.mozilla.org/show_bug.cgi?id=852648#c21
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
    didRemoveDeliveredNotifications:(NSArray*)notifications {
  for (NSUserNotification* notification in notifications) {
    DCHECK(notification);
    auto actionInfo = notifications::mojom::NotificationActionInfo::New();
    // TODO(knollr): Fill |action_info| with details from |notification|.
    _handler->OnNotificationAction(std::move(actionInfo));
  }
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end
