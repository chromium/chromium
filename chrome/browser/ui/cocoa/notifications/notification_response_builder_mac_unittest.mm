// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_operation.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(NotificationOperation::NOTIFICATION_CLICK,
                   NotificationCommon::OPERATION_CLICK);
STATIC_ASSERT_ENUM(NotificationOperation::NOTIFICATION_CLOSE,
                   NotificationCommon::OPERATION_CLOSE);
STATIC_ASSERT_ENUM(NotificationOperation::NOTIFICATION_DISABLE_PERMISSION,
                   NotificationCommon::OPERATION_DISABLE_PERMISSION);
STATIC_ASSERT_ENUM(NotificationOperation::NOTIFICATION_SETTINGS,
                   NotificationCommon::OPERATION_SETTINGS);
STATIC_ASSERT_ENUM(NotificationOperation::NOTIFICATION_OPERATION_MAX,
                   NotificationCommon::OPERATION_MAX);

#undef STATIC_ASSERT_ENUM

class NotificationResponseBuilderMacTest : public testing::Test {
 protected:
  base::scoped_nsobject<NotificationBuilder> NewTestBuilder(
      NotificationHandler::Type type) {
    base::scoped_nsobject<NotificationBuilder> builder(
        [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                           optionsLabel:@"Options"
                                          settingsLabel:@"Settings"]);
    [builder setTitle:@"Title"];
    [builder setSubTitle:@"https://www.miguel.com"];
    [builder setContextMessage:@""];
    [builder setTag:@"tag1"];
    [builder setIcon:[NSImage imageNamed:NSImageNameApplicationIcon]];
    [builder setNotificationId:@"notificationId"];
    [builder setProfileId:@"profileId"];
    [builder setIncognito:false];
    [builder setCreatorPid:@1];
    [builder
        setNotificationType:[NSNumber numberWithInt:static_cast<int>(type)]];
    [builder
        setShowSettingsButton:(type != NotificationHandler::Type::EXTENSION)];
    return builder;
  }
};

TEST_F(NotificationResponseBuilderMacTest, TestNoCreatorPid) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  NSUserNotification* notification = [builder buildUserNotification];
  base::scoped_nsobject<NSMutableDictionary> newUserInfo(
      [[notification userInfo] mutableCopy]);
  [newUserInfo
      removeObjectForKey:notification_constants::kNotificationCreatorPid];
  [notification setUserInfo:newUserInfo];
  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];
  NSNumber* creatorPid =
      [response objectForKey:notification_constants::kNotificationCreatorPid];
  EXPECT_TRUE([creatorPid isEqualToNumber:@0]);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationClick) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  NSUserNotification* notification = [builder buildUserNotification];
  // This will be set by the notification center to indicate the notification
  // was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeContentsClicked)
                  forKey:@"_activationType"];

  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];

  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLICK),
            operation.intValue);
  EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
            buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationSettingsClick) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  NSUserNotification* notification = [builder buildUserNotification];

  // This will be set by the notification center to indicate the only available
  // button was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeActionButtonClicked)
                  forKey:@"_activationType"];
  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];

  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_SETTINGS),
            operation.intValue);
  EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
            buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationOneActionClick) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  [builder setButtons:@"Button1" secondaryButton:@""];

  NSUserNotification* notification = [builder buildUserNotification];

  // These values will be set by the notification center to indicate that button
  // 1 was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeActionButtonClicked)
                  forKey:@"_activationType"];
  [notification setValue:[NSNumber numberWithInt:0]
                  forKey:@"_alternateActionIndex"];
  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLICK),
            operation.intValue);
  EXPECT_EQ(0, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationTwoActionClick) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];

  NSUserNotification* notification = [builder buildUserNotification];

  // These values will be set by the notification center to indicate that button
  // 2 was clicked.
  [notification setValue:@(NSUserNotificationActivationTypeActionButtonClicked)
                  forKey:@"_activationType"];
  [notification setValue:[NSNumber numberWithInt:1]
                  forKey:@"_alternateActionIndex"];

  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLICK),
            operation.intValue);
  EXPECT_EQ(1, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest,
       TestNotificationTwoActionSettingsClick) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];
  NSUserNotification* notification = [builder buildUserNotification];

  // These values will be set by the notification center to indicate that button
  // 2 was clicked.
  [notification
      setValue:
          [NSNumber
              numberWithInt:NSUserNotificationActivationTypeActionButtonClicked]
        forKey:@"_activationType"];
  [notification setValue:[NSNumber numberWithInt:2]
                  forKey:@"_alternateActionIndex"];

  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_SETTINGS),
            operation.intValue);
  EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
            buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationClose) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  NSUserNotification* notification = [builder buildUserNotification];

  // None is what the NSUserNotification center emits when closing since it
  // interprets it as not activated.
  [notification setValue:@(NSUserNotificationActivationTypeNone)
                  forKey:@"_activationType"];

  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLOSE),
            operation.intValue);
  EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
            buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationExtension) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::EXTENSION);
  [builder setButtons:@"Button1" secondaryButton:@"Button2"];
  NSUserNotification* notification = [builder buildUserNotification];
  // These values will be set by the notification center to indicate that button
  // 1 was clicked.
  [notification
      setValue:
          [NSNumber
              numberWithInt:NSUserNotificationActivationTypeActionButtonClicked]
        forKey:@"_activationType"];
  [notification setValue:[NSNumber numberWithInt:1]
                  forKey:@"_alternateActionIndex"];

  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLICK),
            operation.intValue);
  EXPECT_EQ(1, buttonIndex.intValue);
}

TEST_F(NotificationResponseBuilderMacTest, TestNotificationClickAndClose) {
  base::scoped_nsobject<NotificationBuilder> builder =
      NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
  NSUserNotification* notification = [builder buildUserNotification];
  // This will be set by the notification center in didDismissAlert if a
  // notification has been clicked before. Make sure that we do not handle that
  // as a click event. See crbug.com/924414.
  [notification setValue:@(NSUserNotificationActivationTypeContentsClicked)
                  forKey:@"_activationType"];

  NSDictionary* response =
      [NotificationResponseBuilder buildDismissedDictionary:notification];

  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];

  EXPECT_EQ(static_cast<int>(NotificationOperation::NOTIFICATION_CLOSE),
            operation.intValue);
  EXPECT_EQ(notification_constants::kNotificationInvalidButtonIndex,
            buttonIndex.intValue);
}
