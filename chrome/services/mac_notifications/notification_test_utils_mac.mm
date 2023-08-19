// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/notification_test_utils_mac.h"

@implementation FakeUNNotification
@synthesize request = _request;
@end

@implementation FakeUNNotificationSettings
@synthesize alertStyle = _alertStyle;
@synthesize authorizationStatus = _authorizationStatus;
@end

@implementation FakeUNUserNotificationCenter {
  FakeUNNotificationSettings* __strong _settings;
  NSMutableDictionary* __strong _notifications;
  NSSet<UNNotificationCategory*>* __strong _categories;
  id<UNUserNotificationCenterDelegate> __weak _delegate;
}

- (instancetype)init {
  if ((self = [super init])) {
    _settings = [[FakeUNNotificationSettings alloc] init];
    _notifications = [[NSMutableDictionary alloc] init];
    _categories = [[NSSet alloc] init];
    _delegate = nil;
  }
  return self;
}

- (void)setDelegate:(id<UNUserNotificationCenterDelegate>)delegate {
  _delegate = delegate;
}

- (void)removeAllDeliveredNotifications {
  [_notifications removeAllObjects];
}

- (void)setNotificationCategories:(NSSet<UNNotificationCategory*>*)categories {
  _categories = [categories copy];
}

- (void)replaceContentForRequestWithIdentifier:(NSString*)requestIdentifier
                            replacementContent:
                                (UNMutableNotificationContent*)content
                             completionHandler:
                                 (void (^)(NSError* _Nullable error))
                                     notificationDelivered {
  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:requestIdentifier
                                           content:content
                                           trigger:nil];
  FakeUNNotification* notification = [[FakeUNNotification alloc] init];
  notification.request = request;
  [_notifications setObject:notification forKey:request.identifier];
  notificationDelivered(/*error=*/nil);
}

- (void)addNotificationRequest:(UNNotificationRequest*)request
         withCompletionHandler:(void (^)(NSError* error))completionHandler {
  FakeUNNotification* notification = [[FakeUNNotification alloc] init];
  [notification setRequest:request];
  [_notifications setObject:notification forKey:request.identifier];
  completionHandler(/*error=*/nil);
}

- (void)getDeliveredNotificationsWithCompletionHandler:
    (void (^)(NSArray<UNNotification*>* notifications))completionHandler {
  completionHandler([_notifications allValues]);
}

- (void)getNotificationCategoriesWithCompletionHandler:
    (void (^)(NSSet<UNNotificationCategory*>* categories))completionHandler {
  completionHandler([_categories copy]);
}

- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options
                      completionHandler:(void (^)(BOOL granted, NSError* error))
                                            completionHandler {
  completionHandler(/*granted=*/YES, /*error=*/nil);
}

- (void)removeDeliveredNotificationsWithIdentifiers:
    (NSArray<NSString*>*)identifiers {
  [_notifications removeObjectsForKeys:identifiers];
}

- (void)getNotificationSettingsWithCompletionHandler:
    (void (^)(UNNotificationSettings* settings))completionHandler {
  completionHandler(static_cast<UNNotificationSettings*>(_settings));
}

- (FakeUNNotificationSettings*)settings {
  return _settings;
}

- (NSArray<UNNotification*>* _Nonnull)notifications {
  return _notifications.allValues;
}

- (NSSet<UNNotificationCategory*>* _Nonnull)categories {
  return _categories;
}

- (id<UNUserNotificationCenterDelegate> _Nullable)delegate {
  return _delegate;
}

@end
