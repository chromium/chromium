// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_service_ns.h"

#import <Foundation/Foundation.h>
#import <Foundation/NSUserNotification.h>

#include <utility>
#include <vector>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/grit/generated_resources.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#include "chrome/services/mac_notifications/public/cpp/mac_notification_metrics.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

// This class implements the Chromium interface to a deprecated API. It is in
// the process of being replaced, and warnings about its deprecation are not
// helpful. https://crbug.com/1127306
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@interface AlertNSNotificationCenterDelegate
    : NSObject <NSUserNotificationCenterDelegate>
- (instancetype)initWithActionHandler:
    (mojo::PendingRemote<
        mac_notifications::mojom::MacNotificationActionHandler>)handler;
@end

namespace {

NotificationOperation GetNotificationOperationFromNotification(
    NSUserNotification* notification) {
  if (notification.activationType == NSUserNotificationActivationTypeNone) {
    return NotificationOperation::kClose;
  }

  if (notification.activationType !=
      NSUserNotificationActivationTypeActionButtonClicked) {
    return NotificationOperation::kClick;
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

  bool has_settings_button = [[notification.userInfo
      objectForKey:mac_notifications::kNotificationHasSettingsButton]
      boolValue];
  bool clicked_last_button = button_index == button_count - 1;

  // The settings button is always the last button if present.
  if (clicked_last_button && has_settings_button)
    return NotificationOperation::kSettings;
  // Otherwise the user clicked on an action button.
  return NotificationOperation::kClick;
}

int GetActionButtonIndexFromNotification(NSUserNotification* notification) {
  if (notification.activationType !=
          NSUserNotificationActivationTypeActionButtonClicked ||
      GetNotificationOperationFromNotification(notification) !=
          NotificationOperation::kClick) {
    return kNotificationInvalidButtonIndex;
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

void AddActionButtons(
    NSUserNotification* notification,
    const std::vector<mac_notifications::mojom::NotificationActionButtonPtr>&
        buttons,
    bool show_settings_button) {
  DCHECK_LE(buttons.size(), 2u);
  if (![notification respondsToSelector:@selector(_showsButtons)])
    return;

  // Force the notification to always show its action buttons.
  [notification setValue:@YES forKey:@"_showsButtons"];

  NSMutableArray* action_buttons = [NSMutableArray arrayWithCapacity:3];
  for (const auto& button : buttons)
    [action_buttons addObject:base::SysUTF16ToNSString(button->title)];

  if (show_settings_button) {
    // If we can't show an action menu but need a settings button, only show the
    // settings button and don't show developer provided actions.
    if (![notification
            respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]) {
      [action_buttons removeAllObjects];
    }
    [action_buttons
        addObject:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_SETTINGS)];
  }

  if (action_buttons.count == 0) {
    // Don't show action button if no actions needed.
    [notification setHasActionButton:NO];
    return;
  }

  if (action_buttons.count == 1) {
    // Only one action so we don't need a menu. Just set the button title.
    [notification setActionButtonTitle:[action_buttons firstObject]];
    return;
  }

  DCHECK([notification
      respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]);
  DCHECK([notification
      respondsToSelector:@selector(_alternateActionButtonTitles)]);

  // macOS does not support overriding the text of the overflow button and
  // will always show "Options" via this API. Setting actionButtonTitle just
  // appends another button into the overflow menu. Only the new UNNotification
  // API allows overriding this title.
  [notification setValue:@NO forKey:@"_hasActionButton"];

  // Show the alternate menu with developer actions and settings if needed.
  [notification setValue:@YES forKey:@"_alwaysShowAlternateActionMenu"];
  [notification setValue:action_buttons forKey:@"_alternateActionButtonTitles"];
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
      notification_center_(notification_center) {
  notification_center_.delegate = delegate_;
}

MacNotificationServiceNS::~MacNotificationServiceNS() {
  notification_center_.delegate = nil;
}

void MacNotificationServiceNS::DisplayNotification(
    mojom::NotificationPtr notification) {
  NSUserNotification* toast = [[NSUserNotification alloc] init];

  toast.title = base::SysUTF16ToNSString(notification->title);
  toast.subtitle = base::SysUTF16ToNSString(notification->subtitle);
  toast.informativeText = base::SysUTF16ToNSString(notification->body);
  toast.userInfo = GetMacNotificationUserInfo(notification);

  AddActionButtons(toast, notification->buttons,
                   notification->show_settings_button);

  if (!notification->icon.isNull())
    toast.contentImage = gfx::Image(notification->icon).ToNSImage();

  NSString* notification_id =
      base::SysUTF8ToNSString(DeriveMacNotificationId(notification->meta->id));
  toast.identifier = notification_id;

  [notification_center_ deliverNotification:toast];
}

void MacNotificationServiceNS::GetDisplayedNotifications(
    mojom::ProfileIdentifierPtr profile,
    const std::optional<GURL>& origin,
    GetDisplayedNotificationsCallback callback) {
  std::vector<mojom::NotificationIdentifierPtr> notifications;
  // Note: |profile| might be null if we want all notifications.
  NSString* profile_id = profile ? base::SysUTF8ToNSString(profile->id) : nil;
  bool incognito = profile && profile->incognito;

  for (NSUserNotification* toast in notification_center_
           .deliveredNotifications) {
    NSString* toast_id = [toast.userInfo objectForKey:kNotificationId];
    NSString* toast_profile_id =
        [toast.userInfo objectForKey:kNotificationProfileId];
    BOOL toast_incognito =
        [[toast.userInfo objectForKey:kNotificationIncognito] boolValue];

    if (!profile_id || ([profile_id isEqualToString:toast_profile_id] &&
                        incognito == toast_incognito)) {
      NSString* toast_origin_url =
          [toast.userInfo objectForKey:kNotificationOrigin];
      GURL toast_origin = GURL(base::SysNSStringToUTF8(toast_origin_url));
      if (!origin.has_value() || url::IsSameOriginWith(toast_origin, *origin)) {
        auto profile_identifier = mojom::ProfileIdentifier::New(
            base::SysNSStringToUTF8(toast_profile_id), toast_incognito);
        notifications.push_back(mojom::NotificationIdentifier::New(
            base::SysNSStringToUTF8(toast_id), std::move(profile_identifier)));
      }
    }
  }

  std::move(callback).Run(std::move(notifications));
}

void MacNotificationServiceNS::CloseNotification(
    mojom::NotificationIdentifierPtr identifier) {
  NSString* notification_id = base::SysUTF8ToNSString(identifier->id);
  NSString* profile_id = base::SysUTF8ToNSString(identifier->profile->id);
  bool incognito = identifier->profile->incognito;

  for (NSUserNotification* toast in notification_center_
           .deliveredNotifications) {
    NSString* toast_id = [toast.userInfo objectForKey:kNotificationId];
    NSString* toast_profile_id =
        [toast.userInfo objectForKey:kNotificationProfileId];
    BOOL toast_incognito =
        [[toast.userInfo objectForKey:kNotificationIncognito] boolValue];

    if ([notification_id isEqualToString:toast_id] &&
        [profile_id isEqualToString:toast_profile_id] &&
        incognito == toast_incognito) {
      [notification_center_ removeDeliveredNotification:toast];
      break;
    }
  }
}

void MacNotificationServiceNS::CloseNotificationsForProfile(
    mojom::ProfileIdentifierPtr profile) {
  NSString* profile_id = base::SysUTF8ToNSString(profile->id);
  bool incognito = profile->incognito;

  for (NSUserNotification* toast in notification_center_
           .deliveredNotifications) {
    NSString* toast_profile_id =
        [toast.userInfo objectForKey:kNotificationProfileId];
    BOOL toast_incognito =
        [[toast.userInfo objectForKey:kNotificationIncognito] boolValue];

    if ([profile_id isEqualToString:toast_profile_id] &&
        incognito == toast_incognito) {
      [notification_center_ removeDeliveredNotification:toast];
    }
  }
}

void MacNotificationServiceNS::CloseAllNotifications() {
  [notification_center_ removeAllDeliveredNotifications];
}

void MacNotificationServiceNS::OkayToTerminateService(
    OkayToTerminateServiceCallback callback) {
  GetDisplayedNotifications(
      /*profile=*/nullptr, /*origin=*/std::nullopt,
      base::BindOnce([](std::vector<mojom::NotificationIdentifierPtr>
                            notifications) {
        return notifications.empty();
      }).Then(std::move(callback)));
}

}  // namespace mac_notifications

@implementation AlertNSNotificationCenterDelegate {
  // We're using a SharedRemote here as we need to reply on the same sequence
  // that created the mojo connection and the methods below get called by macOS.
  mojo::SharedRemote<mac_notifications::mojom::MacNotificationActionHandler>
      _handler;
}

- (instancetype)initWithActionHandler:
    (mojo::PendingRemote<
        mac_notifications::mojom::MacNotificationActionHandler>)handler {
  if ((self = [super init])) {
    _handler.Bind(std::move(handler), /*bind_task_runner=*/nullptr);
  }
  return self;
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  mac_notifications::mojom::NotificationMetadataPtr meta =
      mac_notifications::GetMacNotificationMetadata(notification.userInfo);
  NotificationOperation operation =
      GetNotificationOperationFromNotification(notification);
  int buttonIndex = GetActionButtonIndexFromNotification(notification);
  auto actionInfo = mac_notifications::mojom::NotificationActionInfo::New(
      std::move(meta), operation, buttonIndex, /*reply=*/std::nullopt);
  _handler->OnNotificationAction(std::move(actionInfo));
}

// Overridden from _NSUserNotificationCenterDelegatePrivate.
// Emitted when a user clicks the "Close" button in the notification.
// It not is emitted if the notification is closed from the notification
// center or if the app is not running at the time the Close button is
// pressed so it's essentially just a best effort way to detect
// notifications closed by the user.
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
               didDismissAlert:(NSUserNotification*)notification {
  mac_notifications::mojom::NotificationMetadataPtr meta =
      mac_notifications::GetMacNotificationMetadata(notification.userInfo);
  auto operation = NotificationOperation::kClose;
  int buttonIndex = kNotificationInvalidButtonIndex;
  auto actionInfo = mac_notifications::mojom::NotificationActionInfo::New(
      std::move(meta), operation, buttonIndex, /*reply=*/std::nullopt);
  _handler->OnNotificationAction(std::move(actionInfo));
}

// Overridden from _NSUserNotificationCenterDelegatePrivate.
// Emitted when a user closes a notification from the notification center.
// This is an undocumented method introduced in 10.8 according to
// https://bugzilla.mozilla.org/show_bug.cgi?id=852648#c21
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
    didRemoveDeliveredNotifications:(NSArray*)notifications {
  for (NSUserNotification* notification in notifications) {
    DCHECK(notification);
    mac_notifications::mojom::NotificationMetadataPtr meta =
        mac_notifications::GetMacNotificationMetadata(notification.userInfo);
    auto operation = NotificationOperation::kClose;
    int buttonIndex = kNotificationInvalidButtonIndex;
    auto actionInfo = mac_notifications::mojom::NotificationActionInfo::New(
        std::move(meta), operation, buttonIndex, /*reply=*/std::nullopt);
    _handler->OnNotificationAction(std::move(actionInfo));
  }
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end

#pragma clang diagnostic pop
