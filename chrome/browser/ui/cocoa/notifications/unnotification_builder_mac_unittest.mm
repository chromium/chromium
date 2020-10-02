// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

API_AVAILABLE(macosx(10.14))
base::scoped_nsobject<UNNotificationBuilder> NewTestBuilder(
    NotificationHandler::Type type) {
  base::scoped_nsobject<UNNotificationBuilder> builder(
      [[UNNotificationBuilder alloc] init]);
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder setCreatorPid:@1];
  [builder setNotificationType:[NSNumber numberWithInt:static_cast<int>(type)]];
  return builder;
}

}  // namespace

TEST(UNNotificationBuilderMacTest, TestNotificationData) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    [builder setTitle:@"Title"];
    [builder setSubTitle:@"https://www.moe.example.com"];
    [builder setContextMessage:@"hey there"];
    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("Title", base::SysNSStringToUTF8([content title]));
    EXPECT_EQ("hey there", base::SysNSStringToUTF8([content body]));
    EXPECT_EQ("https://www.moe.example.com",
              base::SysNSStringToUTF8([content subtitle]));
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationDataMissingContextMessage) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    [builder setContextMessage:@""];
    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("", base::SysNSStringToUTF8([content body]));
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationNoOrigin) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("",
              base::SysNSStringToUTF8([[content userInfo]
                  objectForKey:notification_constants::kNotificationOrigin]));
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationWithOrigin) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    [builder setOrigin:@"example.co.uk"];
    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("example.co.uk",
              base::SysNSStringToUTF8([[content userInfo]
                  objectForKey:notification_constants::kNotificationOrigin]));
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationUserInfo) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];

    NSDictionary* userInfo = [content userInfo];
    EXPECT_EQ("",
              base::SysNSStringToUTF8([userInfo
                  objectForKey:notification_constants::kNotificationOrigin]));
    EXPECT_EQ("notificationId",
              base::SysNSStringToUTF8([userInfo
                  objectForKey:notification_constants::kNotificationId]));
    EXPECT_EQ(
        "profileId",
        base::SysNSStringToUTF8([userInfo
            objectForKey:notification_constants::kNotificationProfileId]));
    EXPECT_FALSE(
        [[userInfo objectForKey:notification_constants::kNotificationIncognito]
            boolValue]);
    EXPECT_TRUE(
        [[userInfo objectForKey:notification_constants::kNotificationCreatorPid]
            isEqualToNumber:@1]);
    EXPECT_TRUE(
        [[userInfo objectForKey:notification_constants::kNotificationType]
            isEqualToNumber:@0]);
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationUserInfoNonDefaultValues) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_NON_PERSISTENT);
    [builder setNotificationId:@"modified id"];
    [builder setOrigin:@"neworigin.co.uk"];
    [builder setProfileId:@"new profile id"];
    [builder setIncognito:true];
    [builder setCreatorPid:@1512];

    UNMutableNotificationContent* content = [builder buildUserNotification];

    NSDictionary* userInfo = [content userInfo];
    EXPECT_EQ("neworigin.co.uk",
              base::SysNSStringToUTF8([userInfo
                  objectForKey:notification_constants::kNotificationOrigin]));
    EXPECT_EQ("modified id",
              base::SysNSStringToUTF8([userInfo
                  objectForKey:notification_constants::kNotificationId]));
    EXPECT_EQ(
        "new profile id",
        base::SysNSStringToUTF8([userInfo
            objectForKey:notification_constants::kNotificationProfileId]));
    EXPECT_TRUE(
        [[userInfo objectForKey:notification_constants::kNotificationIncognito]
            boolValue]);
    EXPECT_TRUE(
        [[userInfo objectForKey:notification_constants::kNotificationCreatorPid]
            isEqualToNumber:@1512]);
    EXPECT_TRUE(
        [[userInfo objectForKey:notification_constants::kNotificationType]
            isEqualToNumber:@1]);
  }
}

TEST(UNNotificationBuilderMacTest, TestBuildDictionary) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> dictionaryBuilder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    [dictionaryBuilder setTitle:@"Title"];
    [dictionaryBuilder setSubTitle:@"https://www.moe.example.com"];
    [dictionaryBuilder setContextMessage:@"hey there"];
    NSDictionary* dictionary = [dictionaryBuilder buildDictionary];

    base::scoped_nsobject<UNNotificationBuilder> builder(
        [[UNNotificationBuilder alloc] initWithDictionary:dictionary]);
    UNMutableNotificationContent* content = [builder buildUserNotification];

    EXPECT_EQ("Title", base::SysNSStringToUTF8([content title]));
    EXPECT_EQ("hey there", base::SysNSStringToUTF8([content body]));
    EXPECT_EQ("https://www.moe.example.com",
              base::SysNSStringToUTF8([content subtitle]));

    NSDictionary* userInfo = [content userInfo];
    EXPECT_EQ("",
              base::SysNSStringToUTF8([userInfo
                  objectForKey:notification_constants::kNotificationOrigin]));
    EXPECT_EQ("notificationId",
              base::SysNSStringToUTF8([userInfo
                  objectForKey:notification_constants::kNotificationId]));
    EXPECT_EQ(
        "profileId",
        base::SysNSStringToUTF8([userInfo
            objectForKey:notification_constants::kNotificationProfileId]));
    EXPECT_FALSE(
        [[userInfo objectForKey:notification_constants::kNotificationIncognito]
            boolValue]);
    EXPECT_TRUE(
        [[userInfo objectForKey:notification_constants::kNotificationCreatorPid]
            isEqualToNumber:@1]);
    EXPECT_TRUE(
        [[userInfo objectForKey:notification_constants::kNotificationType]
            isEqualToNumber:@0]);
  }
}

TEST(UNNotificationBuilderMacTest,
     TestNotificationDoesNotCloseOnDefaultAction) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    UNMutableNotificationContent* content = [builder buildUserNotification];

    EXPECT_TRUE([[content
        valueForKey:@"shouldPreventNotificationDismissalAfterDefaultAction"]
        boolValue]);
  }
}
