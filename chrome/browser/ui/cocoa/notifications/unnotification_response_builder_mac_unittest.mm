// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_operation.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface FakeContent : NSObject
@property(nonatomic, retain) NSDictionary* userInfo;
@end

@implementation FakeContent
@synthesize userInfo;
@end

@interface FakeRequest : NSObject
@property(nonatomic, retain) FakeContent* content;
@end

@implementation FakeRequest
@synthesize content;
@end

@interface FakeUNNotification : NSObject
@property(nonatomic, retain) FakeRequest* request;
@end

@implementation FakeUNNotification
@synthesize request;
@end

@interface FakeUNNotificationResponse : NSObject
@property(nonatomic, retain) FakeUNNotification* notification;
@property(nonatomic, copy) NSString* actionIdentifier;
@end

@implementation FakeUNNotificationResponse
@synthesize notification;
@synthesize actionIdentifier;
@end

namespace {

API_AVAILABLE(macosx(10.14))
base::scoped_nsobject<UNNotificationBuilder> NewTestBuilder(
    NotificationHandler::Type type) {
  base::scoped_nsobject<UNNotificationBuilder> builder(
      [[UNNotificationBuilder alloc] init]);
  [builder setTitle:@"Title"];
  [builder setSubTitle:@"https://www.moe.com"];
  [builder setContextMessage:@"hey there"];
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder setCreatorPid:@1];
  [builder setNotificationType:[NSNumber numberWithInt:static_cast<int>(type)]];
  return builder;
}

API_AVAILABLE(macosx(10.14))
FakeUNNotificationResponse* CreateFakeResponse(NSDictionary* userInfo) {
  FakeContent* fakeContent = [[FakeContent alloc] init];
  fakeContent.userInfo = userInfo;

  FakeRequest* fakeRequest = [[FakeRequest alloc] init];
  fakeRequest.content = fakeContent;

  FakeUNNotification* fakeNotification = [[FakeUNNotification alloc] init];
  fakeNotification.request = fakeRequest;

  FakeUNNotificationResponse* fakeResponse =
      [[FakeUNNotificationResponse alloc] init];
  fakeResponse.actionIdentifier = UNNotificationDefaultActionIdentifier;
  fakeResponse.notification = fakeNotification;

  return fakeResponse;
}

}  // namespace

TEST(UNNotificationResponseBuilderMacTest, TestNoCreatorPid) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> newUserInfo(
        [[content userInfo] mutableCopy]);
    [newUserInfo
        removeObjectForKey:notification_constants::kNotificationCreatorPid];

    FakeUNNotificationResponse* fakeResponse = CreateFakeResponse(newUserInfo);

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse)];
    NSNumber* creatorPid =
        [response objectForKey:notification_constants::kNotificationCreatorPid];
    EXPECT_TRUE([creatorPid isEqualToNumber:@0]);
  }
}

TEST(UNNotificationResponseBuilderMacTest, TestNotificationClick) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> userInfo(
        [[content userInfo] mutableCopy]);

    FakeUNNotificationResponse* fakeResponse = CreateFakeResponse(userInfo);

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse)];

    NSNumber* operation =
        [response objectForKey:notification_constants::kNotificationOperation];
    NSNumber* buttonIndex = [response
        objectForKey:notification_constants::kNotificationButtonIndex];

    EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLICK),
              operation.intValue);
    EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
              buttonIndex.intValue);
  }
}

TEST(UNNotificationResponseBuilderMacTest, TestNotificationClose) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> userInfo(
        [[content userInfo] mutableCopy]);

    FakeUNNotificationResponse* fakeResponse = CreateFakeResponse(userInfo);
    fakeResponse.actionIdentifier = UNNotificationDismissActionIdentifier;

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse)];

    NSNumber* operation =
        [response objectForKey:notification_constants::kNotificationOperation];
    NSNumber* buttonIndex = [response
        objectForKey:notification_constants::kNotificationButtonIndex];

    EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLOSE),
              operation.intValue);
    EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
              buttonIndex.intValue);
  }
}
