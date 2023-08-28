// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_UN_USER_NOTIFICATIONS_SPI_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_UN_USER_NOTIFICATIONS_SPI_H_

#import <UserNotifications/UserNotifications.h>

// Private SPIs exposed by UNUserNotifications.

// Updates the content of an existing notification without triggering a
// renotification. Since macOS 12 this can only be used to update existing
// notifications, in older versions this can also be used to display a new
// notifications.
// This is used so that updated banners do not keep reappearing on
// the screen, for example banners that are used to show progress would keep
// reappearing on the screen without the usage of this private API.
@interface UNUserNotificationCenter (Private)
- (void)replaceContentForRequestWithIdentifier:(NSString*)identifier
                            replacementContent:
                                (UNMutableNotificationContent*)content
                             completionHandler:
                                 (void (^)(NSError* error))completionHandler;
@end

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_UN_USER_NOTIFICATIONS_SPI_H_
