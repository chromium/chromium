// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

namespace notification_constants {

// Applicable to NotificationBuilderBase -> NotificationBuilder,
// UNNotificationBuilder
NSString* const kNotificationTitle = @"title";
NSString* const kNotificationSubTitle = @"subtitle";
NSString* const kNotificationInformativeText = @"informativeText";
NSString* const kNotificationImage = @"icon";
NSString* const kNotificationButtonOne = @"buttonOne";
NSString* const kNotificationButtonTwo = @"buttonTwo";
NSString* const kNotificationTag = @"tag";
NSString* const kNotificationCloseButtonTag = @"closeButton";
NSString* const kNotificationOptionsButtonTag = @"optionsButton";
NSString* const kNotificationSettingsButtonTag = @"settingsButton";

// Applicable to NotificationBuilder and NotificationResponseBuilder
NSString* const kNotificationOrigin = @"notificationOrigin";
NSString* const kNotificationId = @"notificationId";
NSString* const kNotificationProfileId = @"notificationProfileId";
NSString* const kNotificationIncognito = @"notificationIncognito";
NSString* const kNotificationType = @"notificationType";
NSString* const kNotificationHasSettingsButton =
    @"notificationHasSettingsButton";
NSString* const kNotificationCreatorPid = @"notificationCreatorPid";

// Only applicable to the NotificationResponseBuilder
NSString* const kNotificationOperation = @"notificationOperation";
NSString* const kNotificationButtonIndex = @"notificationButtonIndex";

// Name of the XPC service
NSString* const kAlertXPCServiceName = @"%@.framework.AlertNotificationService";

}  // notification_constants
