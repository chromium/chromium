// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/NSUserNotification.h>

#include <string>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#import "chrome/services/mac_notifications/notification_category_manager.h"
#import "chrome/services/mac_notifications/notification_test_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace mac_notifications {

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
  API_AVAILABLE(macos(10.14))
  NSSet<NSString*>* GetCategoryIds() {
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
  base::scoped_nsobject<FakeUNNotification> CreateNotification(
      NSString* identifier,
      UNNotificationCategory* category) {
    base::scoped_nsobject<UNMutableNotificationContent> content(
        [[UNMutableNotificationContent alloc] init]);
    content.get().categoryIdentifier = [category identifier];

    UNNotificationRequest* request =
        [UNNotificationRequest requestWithIdentifier:identifier
                                             content:content.get()
                                             trigger:nil];

    base::scoped_nsobject<FakeUNNotification> notification(
        [[FakeUNNotification alloc] init]);
    [notification setRequest:request];
    return notification;
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
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationSettingsButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] lastObject] identifier]));

      EXPECT_EQ(1ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ("Settings", base::SysNSStringToUTF8(
                                [[[category actions] lastObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationSettingsButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] lastObject] identifier]));

      EXPECT_EQ(1ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Settings", base::SysNSStringToUTF8(
                                [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationSettingsButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] lastObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] lastObject] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    }
  }
}

TEST_F(NotificationCategoryManagerTest, TestNotificationOneButton) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id = manager_->GetOrCreateCategory(
        "notification_id",
        /*buttons=*/{{u"Button1", /*placeholder=*/absl::nullopt}},
        /*settings_button=*/true);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][1] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][1] identifier]));

      EXPECT_EQ(3ul, [[category actions] count]);
    }

    EXPECT_EQ("Settings",
              base::SysNSStringToUTF8([[[category actions] lastObject] title]));
    EXPECT_EQ(
        base::SysNSStringToUTF8(kNotificationSettingsButtonTag),
        base::SysNSStringToUTF8([[[category actions] lastObject] identifier]));

    if ([category respondsToSelector:@selector(actionsMenuTitle)]) {
      EXPECT_EQ("More", base::SysNSStringToUTF8(
                            [category valueForKey:@"_actionsMenuTitle"]));
    }
  }
}

TEST_F(NotificationCategoryManagerTest, TestNotificationTwoButtons) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id = manager_->GetOrCreateCategory(
        "notification_id", /*buttons=*/
        {{u"Button1", /*placeholder=*/absl::nullopt},
         {u"Button2", u"placeholder"}},
        /*settings_button=*/true);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

    UNNotificationAction* action_1 = nullptr;
    UNNotificationAction* action_2 = nullptr;

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      ASSERT_EQ(3ul, [[category actions] count]);
      action_1 = [category actions][0];
      action_2 = [category actions][1];
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      ASSERT_EQ(3ul, [[category actions] count]);
      action_1 = [category actions][0];
      action_2 = [category actions][1];

      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));
    } else {
      ASSERT_EQ(4ul, [[category actions] count]);
      action_1 = [category actions][1];
      action_2 = [category actions][2];

      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));
    }

    EXPECT_EQ("Button1", base::SysNSStringToUTF8([action_1 title]));
    EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
              base::SysNSStringToUTF8([action_1 identifier]));

    EXPECT_EQ("Button2", base::SysNSStringToUTF8([action_2 title]));
    EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonTwo),
              base::SysNSStringToUTF8([action_2 identifier]));

    ASSERT_TRUE([action_2 isKindOfClass:[UNTextInputNotificationAction class]]);
    auto* text_action = static_cast<UNTextInputNotificationAction*>(action_2);
    EXPECT_EQ("Button2",
              base::SysNSStringToUTF8([text_action textInputButtonTitle]));
    EXPECT_EQ("placeholder",
              base::SysNSStringToUTF8([text_action textInputPlaceholder]));

    EXPECT_EQ("Settings",
              base::SysNSStringToUTF8([[[category actions] lastObject] title]));
    EXPECT_EQ(
        base::SysNSStringToUTF8(kNotificationSettingsButtonTag),
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
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ(0ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ(1ul, [[category actions] count]);
    }
  }
}

TEST_F(NotificationCategoryManagerTest, TestNotificationExtensionTwoButtons) {
  if (@available(macOS 10.14, *)) {
    NSString* category_id = manager_->GetOrCreateCategory(
        "notification_id", /*buttons=*/
        {{u"Button1", /*placeholder=*/absl::nullopt},
         {u"Button2", /*placeholder=*/absl::nullopt}},
        /*settings_button=*/false);
    ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
    UNNotificationCategory* category =
        [[fake_notification_center_ categories] anyObject];
    EXPECT_NSEQ(category_id, [category identifier]);

    // Test contents of the category
    if (base::mac::IsAtLeastOS11()) {
      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else if ([category respondsToSelector:@selector(alternateAction)]) {
      EXPECT_EQ("Close", base::SysNSStringToUTF8([[category
                             valueForKey:@"_alternateAction"] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[category valueForKey:@"_alternateAction"] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][0] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][0] identifier]));

      EXPECT_EQ(2ul, [[category actions] count]);
    } else {
      EXPECT_EQ("Close", base::SysNSStringToUTF8(
                             [[[category actions] firstObject] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationCloseButtonTag),
                base::SysNSStringToUTF8(
                    [[[category actions] firstObject] identifier]));

      EXPECT_EQ("Button1",
                base::SysNSStringToUTF8([[category actions][1] title]));
      EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
                base::SysNSStringToUTF8([[category actions][1] identifier]));

      EXPECT_EQ(3ul, [[category actions] count]);
    }

    EXPECT_EQ("Button2",
              base::SysNSStringToUTF8([[[category actions] lastObject] title]));
    EXPECT_EQ(
        base::SysNSStringToUTF8(kNotificationButtonTwo),
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
    manager_->ReleaseCategories({"notification_id_1"});
    // Expect there to be still one category.
    EXPECT_EQ(1u, [[fake_notification_center_ categories] count]);

    // Release category for second notification id.
    manager_->ReleaseCategories({"notification_id_2"});
    // Expect it to release the category.
    EXPECT_EQ(0u, [[fake_notification_center_ categories] count]);
  }
}

TEST_F(NotificationCategoryManagerTest, ReleaseMultipleCategories) {
  if (@available(macOS 10.14, *)) {
    // Request two categories with the same buttons and one with different ones.
    NSString* category_id_1 = manager_->GetOrCreateCategory(
        "notification_id_1", /*buttons=*/{}, /*settings_button=*/false);
    NSString* category_id_2 = manager_->GetOrCreateCategory(
        "notification_id_2", /*buttons=*/{}, /*settings_button=*/false);
    NSString* category_id_3 = manager_->GetOrCreateCategory(
        "notification_id_3", /*buttons=*/{}, /*settings_button=*/true);

    // Expect all category ids to be created.
    EXPECT_NSEQ(category_id_1, category_id_2);
    EXPECT_NSNE(category_id_2, category_id_3);
    NSSet<NSString*>* category_ids = GetCategoryIds();
    EXPECT_EQ(2u, [category_ids count]);
    EXPECT_TRUE([category_ids containsObject:category_id_1]);
    EXPECT_TRUE([category_ids containsObject:category_id_3]);

    // Release categories for the first and third notification ids.
    manager_->ReleaseCategories({"notification_id_1", "notification_id_3"});

    // Expect the first category to still remain but the second one should not.
    NSSet<NSString*>* remaining_category_ids = GetCategoryIds();
    EXPECT_EQ(1u, [remaining_category_ids count]);
    EXPECT_TRUE([remaining_category_ids containsObject:category_id_1]);
  }
}

TEST_F(NotificationCategoryManagerTest, ReleaseAllCategories) {
  if (@available(macOS 10.14, *)) {
    // Request two categories with the same buttons and one with different ones.
    manager_->GetOrCreateCategory("notification_id_1", /*buttons=*/{},
                                  /*settings_button=*/false);
    manager_->GetOrCreateCategory("notification_id_2", /*buttons=*/{},
                                  /*settings_button=*/false);
    manager_->GetOrCreateCategory("notification_id_3", /*buttons=*/{},
                                  /*settings_button=*/true);

    // Expect two unique categories to be created.
    NSSet<NSString*>* category_ids = GetCategoryIds();
    EXPECT_EQ(2u, [category_ids count]);

    // Release all categories and expect them to be gone.
    manager_->ReleaseAllCategories();
    EXPECT_EQ(0u, [[fake_notification_center_ categories] count]);
  }
}

TEST_F(NotificationCategoryManagerTest, InitializeExistingCategories) {
  if (@available(macOS 10.14, *)) {
    NotificationCategoryManager::CategoryKey category_key(
        {{{u"Action", u"Reply"}}, /*settings_button=*/true});
    UNNotificationCategory* category_ns =
        NotificationCategoryManager::CreateCategory(category_key);
    base::scoped_nsobject<FakeUNNotification> notfication =
        CreateNotification(@"identifier1", category_ns);
    auto* notification_ns = static_cast<UNNotification*>(notfication.get());

    base::scoped_nsobject<NSArray<UNNotification*>> notifications(
        [[NSArray alloc] initWithArray:@[ notification_ns ]]);
    base::scoped_nsobject<NSSet<UNNotificationCategory*>> categories(
        [[NSSet alloc] initWithArray:@[ category_ns ]]);

    manager_->InitializeExistingCategories(std::move(notifications),
                                           std::move(categories));

    // Check that we did indeed initialize internal state.
    ASSERT_EQ(1u, manager_->buttons_category_map_.size());
    ASSERT_EQ(1u, manager_->buttons_category_map_.count(category_key));
    ASSERT_EQ(1, manager_->buttons_category_map_[category_key].second);
    ASSERT_EQ(1u, manager_->notification_id_buttons_map_.size());
    ASSERT_EQ(1u, manager_->notification_id_buttons_map_.count("identifier1"));

    // Asking for the same category should return the existing one.
    NSString* category_id = manager_->GetOrCreateCategory(
        "identifier2", category_key.first, category_key.second);
    EXPECT_NSEQ([category_ns identifier], category_id);

    // We should now have both notification ids mapping to the same category.
    ASSERT_EQ(1u, manager_->buttons_category_map_.size());
    ASSERT_EQ(1u, manager_->buttons_category_map_.count(category_key));
    ASSERT_EQ(2, manager_->buttons_category_map_[category_key].second);
    ASSERT_EQ(2u, manager_->notification_id_buttons_map_.size());
    ASSERT_EQ(1u, manager_->notification_id_buttons_map_.count("identifier1"));
    ASSERT_EQ(1u, manager_->notification_id_buttons_map_.count("identifier2"));
    EXPECT_EQ(category_key,
              manager_->notification_id_buttons_map_["identifier1"]);
    EXPECT_EQ(category_key,
              manager_->notification_id_buttons_map_["identifier2"]);
  }
}

}  // namespace mac_notifications
