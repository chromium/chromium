// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"
#include "chrome/services/mac_notifications/public/cpp/notification_test_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

API_AVAILABLE(macosx(10.14))
base::scoped_nsobject<UNNotificationBuilder> NewTestBuilder(
    NotificationHandler::Type type) {
  base::scoped_nsobject<UNNotificationBuilder> builder(
      [[UNNotificationBuilder alloc] initWithCloseLabel:@"Close"
                                           optionsLabel:@"Options"
                                          settingsLabel:@"Settings"]);
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

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(newUserInfo);

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse.get())
              fromAlert:NO];
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

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(userInfo);

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse.get())
              fromAlert:NO];

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

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(userInfo);
    fakeResponse.get().actionIdentifier = UNNotificationDismissActionIdentifier;

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse.get())
              fromAlert:NO];

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

TEST(UNNotificationResponseBuilderMacTest, TestNotificationCloseButton) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> userInfo(
        [[content userInfo] mutableCopy]);

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(userInfo);
    fakeResponse.get().actionIdentifier =
        notification_constants::kNotificationCloseButtonTag;

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse.get())
              fromAlert:NO];

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

TEST(UNNotificationResponseBuilderMacTest, TestNotificationSettingsButton) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> userInfo(
        [[content userInfo] mutableCopy]);

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(userInfo);
    fakeResponse.get().actionIdentifier =
        notification_constants::kNotificationSettingsButtonTag;

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse.get())
              fromAlert:NO];

    NSNumber* operation =
        [response objectForKey:notification_constants::kNotificationOperation];
    NSNumber* buttonIndex = [response
        objectForKey:notification_constants::kNotificationButtonIndex];

    EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_SETTINGS),
              operation.intValue);
    EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
              buttonIndex.intValue);
  }
}

TEST(UNNotificationResponseBuilderMacTest, TestNotificationButtonOne) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> userInfo(
        [[content userInfo] mutableCopy]);

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(userInfo);
    fakeResponse.get().actionIdentifier =
        notification_constants::kNotificationButtonOne;

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse.get())
              fromAlert:NO];

    NSNumber* operation =
        [response objectForKey:notification_constants::kNotificationOperation];
    NSNumber* buttonIndex = [response
        objectForKey:notification_constants::kNotificationButtonIndex];

    EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLICK),
              operation.intValue);
    EXPECT_EQ(0, buttonIndex.intValue);
  }
}

TEST(UNNotificationResponseBuilderMacTest, TestNotificationButtonTwo) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> userInfo(
        [[content userInfo] mutableCopy]);

    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(userInfo);
    fakeResponse.get().actionIdentifier =
        notification_constants::kNotificationButtonTwo;

    NSDictionary* response = [UNNotificationResponseBuilder
        buildDictionary:static_cast<UNNotificationResponse*>(fakeResponse.get())
              fromAlert:NO];

    NSNumber* operation =
        [response objectForKey:notification_constants::kNotificationOperation];
    NSNumber* buttonIndex = [response
        objectForKey:notification_constants::kNotificationButtonIndex];

    EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLICK),
              operation.intValue);
    EXPECT_EQ(1, buttonIndex.intValue);
  }
}

TEST(UNNotificationResponseBuilderMacTest, TestFromAlert) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    base::scoped_nsobject<NSMutableDictionary> userInfo(
        [[content userInfo] mutableCopy]);
    base::scoped_nsobject<FakeUNNotificationResponse> fakeResponse =
        CreateFakeUNNotificationResponse(userInfo);
    UNNotificationResponse* response =
        static_cast<UNNotificationResponse*>(fakeResponse.get());

    EXPECT_NSEQ(@NO,
                [[UNNotificationResponseBuilder buildDictionary:response
                                                      fromAlert:NO]
                    objectForKey:notification_constants::kNotificationIsAlert]);

    EXPECT_NSEQ(@YES,
                [[UNNotificationResponseBuilder buildDictionary:response
                                                      fromAlert:YES]
                    objectForKey:notification_constants::kNotificationIsAlert]);
  }
}
