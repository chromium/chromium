// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_NOTIFICATION_TEST_UTILS_MAC_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_NOTIFICATION_TEST_UTILS_MAC_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

@interface FakeUNNotification : NSObject
@property(nonatomic, strong, nullable) UNNotificationRequest* request;
@end

@interface FakeUNNotificationSettings : NSObject
@property(nonatomic, assign) UNAlertStyle alertStyle;
@property(nonatomic, assign) UNAuthorizationStatus authorizationStatus;
@end

@interface FakeUNUserNotificationCenter : NSObject
- (nullable instancetype)init;
- (void)setDelegate:(id<UNUserNotificationCenterDelegate> _Nullable)delegate;
- (void)removeAllDeliveredNotifications;
- (void)setNotificationCategories:
    (NSSet<UNNotificationCategory*>* _Nonnull)categories;
- (void)replaceContentForRequestWithIdentifier:
            (NSString* _Nonnull)requestIdentifier
                            replacementContent:
                                (UNMutableNotificationContent* _Nonnull)content
                             completionHandler:
                                 (void (^_Nonnull)(NSError* _Nullable error))
                                     notificationDelivered;
- (void)addNotificationRequest:(UNNotificationRequest* _Nonnull)request
         withCompletionHandler:
             (void (^_Nonnull)(NSError* _Nullable error))completionHandler;
- (void)getDeliveredNotificationsWithCompletionHandler:
    (void (^_Nonnull)(NSArray<UNNotification*>* _Nonnull notifications))
        completionHandler;
- (void)getNotificationCategoriesWithCompletionHandler:
    (void (^_Nonnull)(NSSet<UNNotificationCategory*>* _Nonnull categories))
        completionHandler;
- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options
                      completionHandler:
                          (void (^_Nonnull)(BOOL granted,
                                            NSError* _Nullable error))
                              completionHandler;
- (void)removeDeliveredNotificationsWithIdentifiers:
    (NSArray<NSString*>* _Nonnull)identifiers;
- (void)getNotificationSettingsWithCompletionHandler:
    (void (^_Nonnull)(UNNotificationSettings* _Nonnull settings))
        completionHandler;

// Synchronous accessors for tests.
- (FakeUNNotificationSettings* _Nonnull)settings;
- (NSArray<UNNotification*>* _Nonnull)notifications;
- (NSSet<UNNotificationCategory*>* _Nonnull)categories;
- (id<UNUserNotificationCenterDelegate> _Nullable)delegate;

@end

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_NOTIFICATION_TEST_UTILS_MAC_H_
