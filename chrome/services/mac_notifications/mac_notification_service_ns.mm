// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_service_ns.h"

#import <Foundation/Foundation.h>
#import <Foundation/NSUserNotification.h>

#include <utility>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "chrome/services/mac_notifications/mac_notification_service_utils.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"
#include "chrome/services/mac_notifications/public/cpp/notification_utils_mac.h"
#include "mojo/public/cpp/bindings/remote.h"

@interface AlertNSNotificationCenterDelegate
    : NSObject <NSUserNotificationCenterDelegate>
- (instancetype)initWithActionHandler:
    (mojo::PendingRemote<
        mac_notifications::mojom::MacNotificationActionHandler>)handler;
@end

namespace {

NotificationOperation GetNotificationOperationFromNotification(
    NSUserNotification* notification) {
  if ([notification activationType] == NSUserNotificationActivationTypeNone)
    return NotificationOperation::NOTIFICATION_CLOSE;

  if ([notification activationType] !=
      NSUserNotificationActivationTypeActionButtonClicked) {
    return NotificationOperation::NOTIFICATION_CLICK;
  }

  int button_count = 1;
  if ([notification
          respondsToSelector:@selector(_alternateActionButtonTitles)]) {
    int alternate_button_count =
        [[notification valueForKey:@"_alternateActionButtonTitles"] count];
    // We might not need alternateActionButtonTitles if there's only 1 button.
    if (alternate_button_count)
      button_count = alternate_button_count;
  }

  int button_index = 0;
  if (button_count > 1) {
    // There are multiple buttons in the overflow menu. Get the clicked index.
    button_index =
        [[notification valueForKey:@"_alternateActionIndex"] intValue];
  }

  bool has_settings_button = [[[notification userInfo]
      objectForKey:notification_constants::kNotificationHasSettingsButton]
      boolValue];
  bool clicked_last_button = button_index == button_count - 1;

  // The settings button is always the last button if present.
  if (clicked_last_button && has_settings_button)
    return NotificationOperation::NOTIFICATION_SETTINGS;
  // Otherwise the user clicked on an action button.
  return NotificationOperation::NOTIFICATION_CLICK;
}

int GetActionButtonIndexFromNotification(NSUserNotification* notification) {
  if ([notification activationType] !=
          NSUserNotificationActivationTypeActionButtonClicked ||
      GetNotificationOperationFromNotification(notification) !=
          NotificationOperation::NOTIFICATION_CLICK) {
    return notification_constants::kNotificationInvalidButtonIndex;
  }

  // If we couldn't show an overflow menu there's only one button.
  if (![notification
          respondsToSelector:@selector(_alternateActionButtonTitles)]) {
    return 0;
  }

  int alternate_button_count =
      [[notification valueForKey:@"_alternateActionButtonTitles"] count];
  if (alternate_button_count <= 1)
    return 0;

  // There are multiple buttons in the overflow menu. Get the clicked index.
  return [[notification valueForKey:@"_alternateActionIndex"] intValue];
}

}  // namespace

namespace mac_notifications {

MacNotificationServiceNS::MacNotificationServiceNS(
    mojo::PendingReceiver<mojom::MacNotificationService> service,
    mojo::PendingRemote<mojom::MacNotificationActionHandler> handler,
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

void MacNotificationServiceNS::DisplayNotification(
    mojom::NotificationPtr notification) {
  base::scoped_nsobject<NSUserNotification> toast(
      [[NSUserNotification alloc] init]);

  // TODO(knollr): Fill with values from |notification|.
  [toast setTitle:@"title"];
  [toast setSubtitle:@"subtitle"];
  [toast setInformativeText:@"informative"];
  [toast setUserInfo:GetMacNotificationUserInfo(notification)];

  const mojom::NotificationIdentifierPtr& identifier = notification->meta->id;
  NSString* notification_id = base::SysUTF8ToNSString(DeriveMacNotificationId(
      identifier->profile->incognito, identifier->profile->id, identifier->id));
  [toast setIdentifier:notification_id];

  [notification_center_ deliverNotification:toast.get()];
}

void MacNotificationServiceNS::GetDisplayedNotifications(
    mojom::ProfileIdentifierPtr profile,
    GetDisplayedNotificationsCallback callback) {
  std::vector<mojom::NotificationIdentifierPtr> notifications;
  // Note: |profile| might be null if we want all notifications.
  NSString* profile_id = profile ? base::SysUTF8ToNSString(profile->id) : nil;
  bool incognito = profile && profile->incognito;

  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_id =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];
    NSString* toast_profile_id = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL toast_incognito = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if (!profile_id || ([profile_id isEqualToString:toast_profile_id] &&
                        incognito == toast_incognito)) {
      auto profile_identifier = mojom::ProfileIdentifier::New(
          base::SysNSStringToUTF8(toast_profile_id), toast_incognito);
      notifications.push_back(mojom::NotificationIdentifier::New(
          base::SysNSStringToUTF8(toast_id), std::move(profile_identifier)));
    }
  }

  std::move(callback).Run(std::move(notifications));
}

void MacNotificationServiceNS::CloseNotification(
    mojom::NotificationIdentifierPtr identifier) {
  NSString* notification_id = base::SysUTF8ToNSString(identifier->id);
  NSString* profile_id = base::SysUTF8ToNSString(identifier->profile->id);
  bool incognito = identifier->profile->incognito;

  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_id =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];
    NSString* toast_profile_id = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL toast_incognito = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([notification_id isEqualToString:toast_id] &&
        [profile_id isEqualToString:toast_profile_id] &&
        incognito == toast_incognito) {
      [notification_center_ removeDeliveredNotification:toast];
      break;
    }
  }
}

void MacNotificationServiceNS::CloseAllNotifications() {
  [notification_center_ removeAllDeliveredNotifications];
}

}  // namespace mac_notifications

@implementation AlertNSNotificationCenterDelegate {
  mojo::Remote<mac_notifications::mojom::MacNotificationActionHandler> _handler;
}

- (instancetype)initWithActionHandler:
    (mojo::PendingRemote<
        mac_notifications::mojom::MacNotificationActionHandler>)handler {
  if ((self = [super init])) {
    _handler.Bind(std::move(handler));
  }
  return self;
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  mac_notifications::mojom::NotificationMetadataPtr meta =
      mac_notifications::GetMacNotificationMetadata([notification userInfo]);
  NotificationOperation operation =
      GetNotificationOperationFromNotification(notification);
  int buttonIndex = GetActionButtonIndexFromNotification(notification);
  auto actionInfo = mac_notifications::mojom::NotificationActionInfo::New(
      std::move(meta), operation, buttonIndex, /*reply=*/base::nullopt);
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
  mac_notifications::mojom::NotificationMetadataPtr meta =
      mac_notifications::GetMacNotificationMetadata([notification userInfo]);
  auto operation = NotificationOperation::NOTIFICATION_CLOSE;
  int buttonIndex = notification_constants::kNotificationInvalidButtonIndex;
  auto actionInfo = mac_notifications::mojom::NotificationActionInfo::New(
      std::move(meta), operation, buttonIndex, /*reply=*/base::nullopt);
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
    mac_notifications::mojom::NotificationMetadataPtr meta =
        mac_notifications::GetMacNotificationMetadata([notification userInfo]);
    auto operation = NotificationOperation::NOTIFICATION_CLOSE;
    int buttonIndex = notification_constants::kNotificationInvalidButtonIndex;
    auto actionInfo = mac_notifications::mojom::NotificationActionInfo::New(
        std::move(meta), operation, buttonIndex, /*reply=*/base::nullopt);
    _handler->OnNotificationAction(std::move(actionInfo));
  }
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end
