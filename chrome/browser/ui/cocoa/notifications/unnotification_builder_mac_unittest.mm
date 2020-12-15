// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#include "base/files/file_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_image_retainer.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace {

API_AVAILABLE(macosx(10.14))
base::scoped_nsobject<UNNotificationBuilder> NewTestBuilder(
    NotificationHandler::Type type) {
  base::scoped_nsobject<UNNotificationBuilder> builder(
      [[UNNotificationBuilder alloc] initWithCloseLabel:@"Close"
                                           optionsLabel:@"Options"
                                          settingsLabel:@"Settings"]);
  [builder setNotificationId:@"notificationId"];
  [builder setProfileId:@"profileId"];
  [builder setIncognito:false];
  [builder setCreatorPid:@1];
  [builder setShowSettingsButton:NO];
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
    EXPECT_EQ(0ul, [[content attachments] count]);
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationNoButtons) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_NON_PERSISTENT);

    [builder setShowSettingsButton:true];
    UNNotificationCategory* category = [builder buildCategory];

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      EXPECT_EQ("Settings", base::SysNSStringToUTF8(
                                [[[category actions] lastObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationSettingsButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] lastObject] identifier]));

      EXPECT_EQ(1ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ("Settings", base::SysNSStringToUTF8(
                                [[[category actions] lastObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationSettingsButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] lastObject] identifier]));

      EXPECT_EQ(1ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Settings", base::SysNSStringToUTF8(
                                [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationSettingsButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] lastObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] lastObject] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    }

    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("notificationId",
              base::SysNSStringToUTF8([content categoryIdentifier]));
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationOneButton) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_NON_PERSISTENT);

    [builder setButtons:@"Button1" secondaryButton:@""];
    [builder setShowSettingsButton:true];
    UNNotificationCategory* category = [builder buildCategory];

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][1] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][1] identifier]));

      EXPECT_EQ(3ul, [[category actions] count]);
    }

    EXPECT_EQ("Settings",
              base::SysNSStringToUTF8([[[category actions] lastObject] title]));
    EXPECT_EQ(
        base::SysNSStringToUTF8(
            notification_constants::kNotificationSettingsButtonTag),
        base::SysNSStringToUTF8([[[category actions] lastObject] identifier]));

    if ([category respondsToSelector:@selector(actionsMenuTitle)]) {
      EXPECT_EQ("Options", base::SysNSStringToUTF8(
                               [category valueForKey:@"_actionsMenuTitle"]));
    }

    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("notificationId",
              base::SysNSStringToUTF8([content categoryIdentifier]));
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationTwoButtons) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_NON_PERSISTENT);

    [builder setButtons:@"Button1" secondaryButton:@"Button2"];
    [builder setShowSettingsButton:true];
    UNNotificationCategory* category = [builder buildCategory];

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ("Button2",
                base::SysNSStringToUTF8([[category actions][1] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonTwo),
                base::SysNSStringToUTF8([[category actions][1] identifier]));

      EXPECT_EQ(3ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ("Button2",
                base::SysNSStringToUTF8([[category actions][1] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonTwo),
                base::SysNSStringToUTF8([[category actions][1] identifier]));

      EXPECT_EQ(3ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][1] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][1] identifier]));

      EXPECT_EQ("Button2",
                base::SysNSStringToUTF8([[category actions][2] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonTwo),
                base::SysNSStringToUTF8([[category actions][2] identifier]));

      EXPECT_EQ(4ul, [[category actions] count]);
    }

    EXPECT_EQ("Settings",
              base::SysNSStringToUTF8([[[category actions] lastObject] title]));
    EXPECT_EQ(
        base::SysNSStringToUTF8(
            notification_constants::kNotificationSettingsButtonTag),
        base::SysNSStringToUTF8([[[category actions] lastObject] identifier]));

    if ([category respondsToSelector:@selector(actionsMenuTitle)]) {
      EXPECT_EQ("Options", base::SysNSStringToUTF8(
                               [category valueForKey:@"_actionsMenuTitle"]));
    }

    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("notificationId",
              base::SysNSStringToUTF8([content categoryIdentifier]));
    EXPECT_EQ(0ul, [[content attachments] count]);
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationExtensionNoButtons) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::EXTENSION);

    UNNotificationCategory* category = [builder buildCategory];

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      EXPECT_EQ(0ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ(0ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ(1ul, [[category actions] count]);
    }

    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("notificationId",
              base::SysNSStringToUTF8([content categoryIdentifier]));
  }
}

TEST(UNNotificationBuilderMacTest, TestNotificationExtensionTwoButtons) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::EXTENSION);

    [builder setButtons:@"Button1" secondaryButton:@"Button2"];
    UNNotificationCategory* category = [builder buildCategory];

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][1] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(
                    notification_constants::kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][1] identifier]));

      EXPECT_EQ(3ul, [[category actions] count]);
    }

    EXPECT_EQ("Button2",
              base::SysNSStringToUTF8([[[category actions] lastObject] title]));
    EXPECT_EQ(
        base::SysNSStringToUTF8(notification_constants::kNotificationButtonTwo),
        base::SysNSStringToUTF8([[[category actions] lastObject] identifier]));

    if ([category respondsToSelector:@selector(actionsMenuTitle)]) {
      EXPECT_EQ("Options", base::SysNSStringToUTF8(
                               [category valueForKey:@"_actionsMenuTitle"]));
    }

    UNMutableNotificationContent* content = [builder buildUserNotification];
    EXPECT_EQ("notificationId",
              base::SysNSStringToUTF8([content categoryIdentifier]));
    EXPECT_EQ(0ul, [[content attachments] count]);
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

TEST(UNNotificationBuilderMacTest, TestIcon) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);

    base::test::TaskEnvironment task_environment(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    base::ScopedPathOverride user_data_dir_override(chrome::DIR_USER_DATA);

    auto image_retainer = std::make_unique<NotificationImageRetainer>(
        task_environment.GetMainThreadTaskRunner(),
        task_environment.GetMockTickClock());

    SkBitmap icon;
    icon.allocN32Pixels(64, 64);
    icon.eraseARGB(255, 100, 150, 200);
    gfx::Image image = gfx::Image::CreateFrom1xBitmap(icon);

    base::FilePath temp_file = image_retainer->RegisterTemporaryImage(image);
    ASSERT_FALSE(temp_file.empty());
    ASSERT_TRUE(base::PathExists(temp_file));

    [builder setIconPath:base::SysUTF8ToNSString(temp_file.value())];

    UNMutableNotificationContent* content = [builder buildUserNotification];

    EXPECT_EQ(1ul, [[content attachments] count]);
  }
}

#if defined(ARCH_CPU_ARM64)
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
#define MAYBE_TestIconWrongPath DISABLED_TestIconWrongPath
#else
#define MAYBE_TestIconWrongPath TestIconWrongPath
#endif

TEST(UNNotificationBuilderMacTest, MAYBE_TestIconWrongPath) {
  if (@available(macOS 10.14, *)) {
    base::scoped_nsobject<UNNotificationBuilder> builder =
        NewTestBuilder(NotificationHandler::Type::WEB_PERSISTENT);
    [builder setIconPath:@"wrong-path"];
    UNMutableNotificationContent* content = [builder buildUserNotification];

    if (base::mac::IsAtLeastOS11()) {
      // TODO(knollr): Figure out why macOS 11 allows creating a
      // UNNotificationAttachment with an invalid path.
      EXPECT_EQ(1ul, [[content attachments] count]);
    } else {
      EXPECT_EQ(0ul, [[content attachments] count]);
    }
  }
}
