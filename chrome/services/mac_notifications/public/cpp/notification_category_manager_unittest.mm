// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/NSUserNotification.h>

#include <string>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/services/mac_notifications/public/cpp/notification_category_manager.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_test_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class NotificationCategoryManagerTest : public testing::Test {
 public:
  NotificationCategoryManagerTest() {
    if (@available(macOS 10.14, *)) {
      fake_notification_center_.reset(
          [[FakeUNUserNotificationCenter alloc] init]);
      manager_ = std::make_unique<NotificationCategoryManager>(
          static_cast<UNUserNotificationCenter*>(
              fake_notification_center_.get()));
    }
  }

  ~NotificationCategoryManagerTest() override = default;

 protected:
  NSSet<NSString*>* GetCategoryIds() API_AVAILABLE(macos(10.14)) {
    NSSet<UNNotificationCategory*>* categories =
        [fake_notification_center_ categories];
    NSMutableSet<NSString*>* category_ids =
        [NSMutableSet setWithCapacity:[categories count]];
    for (UNNotificationCategory* category in categories) {
      [category_ids addObject:[category identifier]];
    }
    return category_ids;
  }

  API_AVAILABLE(macos(10.14))
  base::scoped_nsobject<FakeUNUserNotificationCenter> fake_notification_center_;
  API_AVAILABLE(macos(10.14))
  std::unique_ptr<NotificationCategoryManager> manager_;
};

TEST_F(NotificationCategoryManagerTest, TestNotificationNoButtons) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id = manager_->GetOrCreateCategory(
        "notification_id", /*buttons=*/{}, /*settings_button=*/true);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

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
  }
}

TEST_F(NotificationCategoryManagerTest, TestNotificationOneButton) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id = manager_->GetOrCreateCategory(
        "notification_id", /*buttons=*/{u"Button1"},
        /*settings_button=*/true);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

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
      EXPECT_EQ("More", base::SysNSStringToUTF8(
                            [category valueForKey:@"_actionsMenuTitle"]));
    }
  }
}

TEST_F(NotificationCategoryManagerTest, TestNotificationTwoButtons) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id =
        manager_->GetOrCreateCategory("notification_id", /*buttons=*/
                                      {u"Button1", u"Button2"},
                                      /*settings_button=*/true);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

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
      EXPECT_EQ("More", base::SysNSStringToUTF8(
                            [category valueForKey:@"_actionsMenuTitle"]));
    }
  }
}

TEST_F(NotificationCategoryManagerTest, TestNotificationExtensionNoButtons) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id = manager_->GetOrCreateCategory(
        "notification_id", /*buttons=*/{}, /*settings_button=*/false);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

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
  }
}

TEST_F(NotificationCategoryManagerTest, TestNotificationExtensionTwoButtons) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id =
        manager_->GetOrCreateCategory("notification_id", /*buttons=*/
                                      {u"Button1", u"Button2"},
                                      /*settings_button=*/false);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

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
      EXPECT_EQ("More", base::SysNSStringToUTF8(
                            [category valueForKey:@"_actionsMenuTitle"]));
    }
  }
}

TEST_F(NotificationCategoryManagerTest, CreateTwoCategories) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id_1 = manager_->GetOrCreateCategory(
        "notification_id_1", /*buttons=*/{}, /*settings_button=*/false);
    NSString* category_id_2 = manager_->GetOrCreateCategory(
        "notification_id_2", /*buttons=*/{}, /*settings_button=*/true);
    EXPECT_NSNE(category_id_1, category_id_2);

    NSSet<NSString*>* category_ids = GetCategoryIds();
    EXPECT_EQ(2u, [category_ids count]);
    EXPECT_TRUE([category_ids containsObject:category_id_1]);
    EXPECT_TRUE([category_ids containsObject:category_id_2]);
  }
}

TEST_F(NotificationCategoryManagerTest, ReusesCategory) {
  if (@available(macOS 10.14, *)) {
    // Request two categories with the same buttons.
    NSString* category_id_1 = manager_->GetOrCreateCategory(
        "notification_id_1", /*buttons=*/{}, /*settings_button=*/false);
    NSString* category_id_2 = manager_->GetOrCreateCategory(
        "notification_id_2", /*buttons=*/{}, /*settings_button=*/false);

    // Expect both category ids to point to the same category.
    EXPECT_NSEQ(category_id_1, category_id_2);
    // Expect only one category to be created.
    NSSet<NSString*>* category_ids = GetCategoryIds();
    EXPECT_EQ(1u, [category_ids count]);
    EXPECT_TRUE([category_ids containsObject:category_id_1]);

    // Release category for first notification id.
    manager_->ReleaseCategory("notification_id_1");
    // Expect there to be still one category.
    EXPECT_EQ(1u, [[fake_notification_center_ categories] count]);

    // Release category for second notification id.
    manager_->ReleaseCategory("notification_id_2");
    // Expect it to release the category.
    EXPECT_EQ(0u, [[fake_notification_center_ categories] count]);
  }
}
