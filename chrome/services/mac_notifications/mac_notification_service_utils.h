// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UTILS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UTILS_H_

#import <Foundation/Foundation.h>

#include <string>

#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

namespace mac_notifications {

NSDictionary* GetMacNotificationUserInfo(
    const mojom::NotificationPtr& notification);

mojom::NotificationMetadataPtr GetMacNotificationMetadata(
    NSDictionary* user_info);

// Derives a unique notification identifier to be used by the macOS system
// notification center to uniquely identify a notification.
std::string DeriveMacNotificationId(
    const mojom::NotificationIdentifierPtr& identifier);

extern NSString* const kNotificationButtonOne;
extern NSString* const kNotificationButtonTwo;
extern NSString* const kNotificationCloseButtonTag;
extern NSString* const kNotificationHasSettingsButton;
extern NSString* const kNotificationId;
extern NSString* const kNotificationIncognito;
extern NSString* const kNotificationOrigin;
extern NSString* const kNotificationProfileId;
extern NSString* const kNotificationSettingsButtonTag;
extern NSString* const kNotificationType;
extern NSString* const kNotificationUserDataDir;

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UTILS_H_
