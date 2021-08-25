// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_service_un.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#include "chrome/services/mac_notifications/unnotification_metrics.h"
#include "ui/gfx/image/image.h"

// This uses a private API so that updated banners do not keep reappearing on
// the screen, for example banners that are used to show progress would keep
// reappearing on the screen without the usage of this private API.
API_AVAILABLE(macosx(10.14))
@interface UNUserNotificationCenter (Private)
- (void)replaceContentForRequestWithIdentifier:(NSString*)identifier
                            replacementContent:
                                (UNMutableNotificationContent*)content
                             completionHandler:
                                 (void (^)(NSError* _Nullable error))
                                     completionHandler;
@end

API_AVAILABLE(macosx(10.14))
@interface AlertUNNotificationCenterDelegate
    : NSObject <UNUserNotificationCenterDelegate>
- (instancetype)initWithActionHandler:
    (base::RepeatingCallback<
        void(mac_notifications::mojom::NotificationActionInfoPtr)>)handler;
@end

namespace {

API_AVAILABLE(macosx(10.14))
NotificationOperation GetNotificationOperationFromAction(
    NSString* actionIdentifier) {
  if ([actionIdentifier isEqual:UNNotificationDismissActionIdentifier] ||
      [actionIdentifier
          isEqualToString:mac_notifications::kNotificationCloseButtonTag]) {
    return NotificationOperation::kClose;
  }
  if ([actionIdentifier isEqual:UNNotificationDefaultActionIdentifier] ||
      [actionIdentifier
          isEqualToString:mac_notifications::kNotificationButtonOne] ||
      [actionIdentifier
          isEqualToString:mac_notifications::kNotificationButtonTwo]) {
    return NotificationOperation::kClick;
  }
  if ([actionIdentifier
          isEqualToString:mac_notifications::kNotificationSettingsButtonTag]) {
    return NotificationOperation::kSettings;
  }
  NOTREACHED();
  return NotificationOperation::kClick;
}

int GetActionButtonIndexFromAction(NSString* actionIdentifier) {
  if ([actionIdentifier
          isEqualToString:mac_notifications::kNotificationButtonOne]) {
    return 0;
  }
  if ([actionIdentifier
          isEqualToString:mac_notifications::kNotificationButtonTwo]) {
    return 1;
  }
  return kNotificationInvalidButtonIndex;
}

}  // namespace

namespace mac_notifications {

MacNotificationServiceUN::MacNotificationServiceUN(
    mojo::PendingReceiver<mojom::MacNotificationService> service,
    mojo::PendingRemote<mojom::MacNotificationActionHandler> handler,
    UNUserNotificationCenter* notification_center)
    : binding_(this, std::move(service)),
      action_handler_(std::move(handler)),
      notification_center_([notification_center retain]),
      category_manager_(notification_center) {
  delegate_.reset([[AlertUNNotificationCenterDelegate alloc]
      initWithActionHandler:base::BindRepeating(
                                &MacNotificationServiceUN::OnNotificationAction,
                                weak_factory_.GetWeakPtr())]);
  [notification_center_ setDelegate:delegate_.get()];
  LogUNNotificationSettings(notification_center_.get());
  // TODO(crbug.com/1129366): Determine when to ask for permissions.
  RequestPermission();
}

MacNotificationServiceUN::~MacNotificationServiceUN() {
  [notification_center_ setDelegate:nil];
}

void MacNotificationServiceUN::DisplayNotification(
    mojom::NotificationPtr notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::scoped_nsobject<UNMutableNotificationContent> content(
      [[UNMutableNotificationContent alloc] init]);

  [content setTitle:base::SysUTF16ToNSString(notification->title)];
  [content setSubtitle:base::SysUTF16ToNSString(notification->subtitle)];
  [content setBody:base::SysUTF16ToNSString(notification->body)];
  [content setUserInfo:GetMacNotificationUserInfo(notification)];

  std::string notification_id = DeriveMacNotificationId(notification->meta->id);
  NSString* notification_id_ns = base::SysUTF8ToNSString(notification_id);

  // TODO(knollr): Also pass placeholder once we support inline replies.
  NotificationCategoryManager::Buttons buttons;
  for (const auto& button : notification->buttons)
    buttons.push_back(button->title);

  NSString* category_id = category_manager_.GetOrCreateCategory(
      notification_id, buttons, notification->show_settings_button);
  [content setCategoryIdentifier:category_id];

  if (!notification->icon.isNull()) {
    gfx::Image icon(notification->icon);
    base::FilePath path = image_retainer_.RegisterTemporaryImage(icon);
    NSURL* url = [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
    // When the files are saved using NotificationImageRetainer, they're saved
    // without the .png extension. So |options| here is used to tell the system
    // that the file is of type PNG, as NotificationImageRetainer converts files
    // to PNG before writing them.
    NSDictionary* options = @{
      UNNotificationAttachmentOptionsTypeHintKey :
          (__bridge NSString*)kUTTypePNG
    };
    UNNotificationAttachment* attachment =
        [UNNotificationAttachment attachmentWithIdentifier:notification_id_ns
                                                       URL:url
                                                   options:options
                                                     error:nil];

    if (attachment != nil)
      [content setAttachments:@[ attachment ]];
  }

  // This uses a private API to prevent notifications from dismissing after
  // clicking on them. This only affects the default action though, other action
  // buttons will still dismiss the notification on click.
  if ([content respondsToSelector:@selector
               (shouldPreventNotificationDismissalAfterDefaultAction)]) {
    [content setValue:@YES
               forKey:@"shouldPreventNotificationDismissalAfterDefaultAction"];
  }

  auto completion_handler = ^(NSError* _Nullable error) {
    // TODO(knollr): Add UMA logging for display errors.
    if (error != nil) {
      DVLOG(1) << "Displaying notification did not succeed";
    }
  };

  // If the renotify is not set try to replace the notification silently.
  bool should_replace = !notification->renotify;
  bool can_replace = [notification_center_
      respondsToSelector:@selector
      (replaceContentForRequestWithIdentifier:
                           replacementContent:completionHandler:)];
  if (should_replace && can_replace) {
    // If the notification has been delivered before, it will get updated in the
    // notification center. If it hasn't been delivered before it will deliver
    // it and show it on the screen.
    [notification_center_
        replaceContentForRequestWithIdentifier:notification_id_ns
                            replacementContent:content.get()
                             completionHandler:completion_handler];
    return;
  }

  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:notification_id_ns
                                           content:content.get()
                                           trigger:nil];

  [notification_center_ addNotificationRequest:request
                         withCompletionHandler:completion_handler];
}

void MacNotificationServiceUN::GetDisplayedNotifications(
    mojom::ProfileIdentifierPtr profile,
    GetDisplayedNotificationsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    std::vector<mojom::NotificationIdentifierPtr> notifications;

    for (UNNotification* toast in toasts) {
      NSDictionary* user_info = [[[toast request] content] userInfo];
      NSString* toast_id = [user_info objectForKey:kNotificationId];
      NSString* toast_profile_id =
          [user_info objectForKey:kNotificationProfileId];
      bool toast_incognito =
          [[user_info objectForKey:kNotificationIncognito] boolValue];

      if (!profile_id || ([profile_id isEqualToString:toast_profile_id] &&
                          incognito == toast_incognito)) {
        auto profile_identifier = mojom::ProfileIdentifier::New(
            base::SysNSStringToUTF8(toast_profile_id), toast_incognito);
        notifications.push_back(mojom::NotificationIdentifier::New(
            base::SysNSStringToUTF8(toast_id), std::move(profile_identifier)));
      }
    }

    task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(block_callback),
                                                    std::move(notifications)));
  }];
}

void MacNotificationServiceUN::CloseNotification(
    mojom::NotificationIdentifierPtr identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string notification_id = DeriveMacNotificationId(identifier);
  NSString* notification_id_ns = base::SysUTF8ToNSString(notification_id);
  [notification_center_
      removeDeliveredNotificationsWithIdentifiers:@[ notification_id_ns ]];
  OnNotificationsClosed({notification_id});
}

void MacNotificationServiceUN::CloseNotificationsForProfile(
    mojom::ProfileIdentifierPtr profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSString* profile_id = base::SysUTF8ToNSString(profile->id);
  bool incognito = profile->incognito;

  __block auto closed_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&MacNotificationServiceUN::OnNotificationsClosed,
                     weak_factory_.GetWeakPtr()));

  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull toasts) {
    base::scoped_nsobject<NSMutableArray> identifiers(
        [[NSMutableArray alloc] init]);
    std::vector<std::string> closed_notification_ids;

    for (UNNotification* toast in toasts) {
      NSDictionary* user_info = [[[toast request] content] userInfo];
      NSString* toast_id = [[toast request] identifier];
      NSString* toast_profile_id =
          [user_info objectForKey:kNotificationProfileId];
      bool toast_incognito =
          [[user_info objectForKey:kNotificationIncognito] boolValue];

      if ([profile_id isEqualToString:toast_profile_id] &&
          incognito == toast_incognito) {
        [identifiers addObject:toast_id];
        closed_notification_ids.push_back(base::SysNSStringToUTF8(toast_id));
      }
    }

    [notification_center_
        removeDeliveredNotificationsWithIdentifiers:identifiers];
    std::move(closed_callback).Run(closed_notification_ids);
  }];
}

void MacNotificationServiceUN::CloseAllNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [notification_center_ removeAllDeliveredNotifications];
  category_manager_.ReleaseAllCategories();
}

void MacNotificationServiceUN::RequestPermission() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void MacNotificationServiceUN::OnNotificationAction(
    mojom::NotificationActionInfoPtr action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (action->operation == NotificationOperation::kClose)
    OnNotificationsClosed({DeriveMacNotificationId(action->meta->id)});

  action_handler_->OnNotificationAction(std::move(action));
}

void MacNotificationServiceUN::OnNotificationsClosed(
    const std::vector<std::string>& notification_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  category_manager_.ReleaseCategories(notification_ids);
}

}  // namespace mac_notifications

@implementation AlertUNNotificationCenterDelegate {
  base::RepeatingCallback<void(
      mac_notifications::mojom::NotificationActionInfoPtr)>
      _handler;
}

- (instancetype)initWithActionHandler:
    (base::RepeatingCallback<
        void(mac_notifications::mojom::NotificationActionInfoPtr)>)handler {
  if ((self = [super init])) {
    // We're binding to the current sequence here as we need to reply on the
    // same sequence and the methods below get called by macOS.
    _handler = base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                  std::move(handler));
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
  mac_notifications::mojom::NotificationMetadataPtr meta =
      mac_notifications::GetMacNotificationMetadata(
          [[[[response notification] request] content] userInfo]);
  NotificationOperation operation =
      GetNotificationOperationFromAction([response actionIdentifier]);
  int buttonIndex = GetActionButtonIndexFromAction([response actionIdentifier]);
  auto actionInfo = mac_notifications::mojom::NotificationActionInfo::New(
      std::move(meta), operation, buttonIndex, /*reply=*/absl::nullopt);
  _handler.Run(std::move(actionInfo));
  completionHandler();
}

@end
