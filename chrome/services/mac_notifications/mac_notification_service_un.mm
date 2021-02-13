// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_service_un.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_utils_mac.h"
#include "mojo/public/cpp/bindings/remote.h"

API_AVAILABLE(macosx(10.14))
@interface AlertUNNotificationCenterDelegate
    : NSObject <UNUserNotificationCenterDelegate>
- (instancetype)initWithActionHandler:
    (mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>)
        handler;
@end

MacNotificationServiceUN::MacNotificationServiceUN(
    mojo::PendingReceiver<notifications::mojom::MacNotificationService> service,
    mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>
        handler,
    UNUserNotificationCenter* notification_center)
    : binding_(this, std::move(service)),
      delegate_([[AlertUNNotificationCenterDelegate alloc]
          initWithActionHandler:std::move(handler)]),
      notification_center_([notification_center retain]) {
  [notification_center_ setDelegate:delegate_.get()];
  // TODO(crbug.com/1129366): Determine when to ask for permissions.
  RequestPermission();
}

MacNotificationServiceUN::~MacNotificationServiceUN() {
  [notification_center_ setDelegate:nil];
}

void MacNotificationServiceUN::GetDisplayedNotifications(
    notifications::mojom::ProfileIdentifierPtr profile,
    GetDisplayedNotificationsCallback callback) {
  // Move |callback| into block storage so we can use it from the block below.
  __block GetDisplayedNotificationsCallback block_callback =
      std::move(callback);

  // Note: |profile| might be null if we want all notifications.
  NSString* profile_id = profile ? base::SysUTF8ToNSString(profile->id) : nil;
  bool incognito = profile && profile->incognito;

  // We need to call |callback| on the same sequence as this method is called.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();

  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull toasts) {
    std::vector<notifications::mojom::NotificationIdentifierPtr> notifications;

    for (UNNotification* toast in toasts) {
      NSDictionary* user_info = [[[toast request] content] userInfo];
      NSString* toast_id =
          [user_info objectForKey:notification_constants::kNotificationId];
      NSString* toast_profile_id = [user_info
          objectForKey:notification_constants::kNotificationProfileId];
      bool toast_incognito = [[user_info
          objectForKey:notification_constants::kNotificationIncognito]
          boolValue];

      if (!profile_id || ([profile_id isEqualToString:toast_profile_id] &&
                          incognito == toast_incognito)) {
        auto profile_identifier = notifications::mojom::ProfileIdentifier::New(
            base::SysNSStringToUTF8(toast_profile_id), toast_incognito);
        notifications.push_back(
            notifications::mojom::NotificationIdentifier::New(
                base::SysNSStringToUTF8(toast_id),
                std::move(profile_identifier)));
      }
    }

    task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(block_callback),
                                                    std::move(notifications)));
  }];
}

void MacNotificationServiceUN::CloseNotification(
    notifications::mojom::NotificationIdentifierPtr identifier) {
  NSString* notification_id = base::SysUTF8ToNSString(DeriveMacNotificationId(
      identifier->profile->incognito, identifier->profile->id, identifier->id));
  [notification_center_
      removeDeliveredNotificationsWithIdentifiers:@[ notification_id ]];
}

void MacNotificationServiceUN::CloseAllNotifications() {
  [notification_center_ removeAllDeliveredNotifications];
}

void MacNotificationServiceUN::RequestPermission() {
  UNAuthorizationOptions authOptions = UNAuthorizationOptionAlert |
                                       UNAuthorizationOptionSound |
                                       UNAuthorizationOptionBadge;

  auto resultHandler = ^(BOOL granted, NSError* _Nullable error) {
    // TODO(knollr): Add UMA logging for permission status.
    if (error != nil) {
      DVLOG(1) << "Requesting permission did not succeed";
    }
  };

  [notification_center_ requestAuthorizationWithOptions:authOptions
                                      completionHandler:resultHandler];
}

@implementation AlertUNNotificationCenterDelegate {
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

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
  // Receiving a notification when the app is in the foreground.
  UNNotificationPresentationOptions presentationOptions =
      UNNotificationPresentationOptionSound |
      UNNotificationPresentationOptionAlert |
      UNNotificationPresentationOptionBadge;
  completionHandler(presentationOptions);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler {
  auto actionInfo = notifications::mojom::NotificationActionInfo::New();
  // TODO(knollr): Fill |action_info| with details from |response|.
  _handler->OnNotificationAction(std::move(actionInfo));
  completionHandler();
}

@end
