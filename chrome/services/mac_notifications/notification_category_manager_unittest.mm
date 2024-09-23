// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/notification_category_manager.h"

#import <Foundation/NSUserNotification.h>

#include <optional>
#include <string>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#import "chrome/services/mac_notifications/notification_test_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace mac_notifications {

class NotificationCategoryManagerTest : public testing::Test {
 public:
  NotificationCategoryManagerTest() {
    fake_notification_center_ = [[FakeUNUserNotificationCenter alloc] init];
    manager_ = std::make_unique<NotificationCategoryManager>(
        static_cast<UNUserNotificationCenter*>(fake_notification_center_));
  }

  ~NotificationCategoryManagerTest() override = default;

 protected:
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

  FakeUNNotification* CreateNotification(NSString* identifier,
                                         UNNotificationCategory* category) {
    UNMutableNotificationContent* content =
        [[UNMutableNotificationContent alloc] init];
    content.categoryIdentifier = category.identifier;

    UNNotificationRequest* request =
        [UNNotificationRequest requestWithIdentifier:identifier
                                             content:content
                                             trigger:nil];

    FakeUNNotification* notification = [[FakeUNNotification alloc] init];
    notification.request = request;
    return notification;
  }

  FakeUNUserNotificationCenter* __strong fake_notification_center_;
  std::unique_ptr<NotificationCategoryManager> manager_;
};

TEST_F(NotificationCategoryManagerTest, TestNotificationNoButtons) {
  NSString* category_id = manager_->GetOrCreateCategory(
      "notification_id", /*buttons=*/{}, /*settings_button=*/true);
  ASSERT_EQ(1u, fake_notification_center_.categories.count);
  UNNotificationCategory* category =
      [[fake_notification_center_ categories] anyObject];
  EXPECT_NSEQ(category_id, [category identifier]);

  // Test contents of the category
  EXPECT_EQ("Settings",
            base::SysNSStringToUTF8([[[category actions] lastObject] title]));
  EXPECT_EQ(
      base::SysNSStringToUTF8(kNotificationSettingsButtonTag),
      base::SysNSStringToUTF8([[[category actions] lastObject] identifier]));

  EXPECT_EQ(1ul, [[category actions] count]);
}

TEST_F(NotificationCategoryManagerTest, TestNotificationOneButton) {
  NSString* category_id = manager_->GetOrCreateCategory(
      "notification_id",
      /*buttons=*/{{u"Button1", /*placeholder=*/std::nullopt}},
      /*settings_button=*/true);
  ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
  UNNotificationCategory* category =
      [[fake_notification_center_ categories] anyObject];
  EXPECT_NSEQ(category_id, [category identifier]);

  // Test contents of the category
  EXPECT_EQ("Button1", base::SysNSStringToUTF8([[category actions][0] title]));
  EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
            base::SysNSStringToUTF8([[category actions][0] identifier]));

  EXPECT_EQ(2ul, [[category actions] count]);

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

TEST_F(NotificationCategoryManagerTest, TestNotificationTwoButtons) {
  NSString* category_id =
      manager_->GetOrCreateCategory("notification_id", /*buttons=*/
                                    {{u"Button1", /*placeholder=*/std::nullopt},
                                     {u"Button2", u"placeholder"}},
                                    /*settings_button=*/true);
  ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
  UNNotificationCategory* category =
      [[fake_notification_center_ categories] anyObject];
  EXPECT_NSEQ(category_id, [category identifier]);

  UNNotificationAction* action_1 = nullptr;
  UNNotificationAction* action_2 = nullptr;

  // Test contents of the category
  ASSERT_EQ(3ul, [[category actions] count]);
  action_1 = [category actions][0];
  action_2 = [category actions][1];

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

TEST_F(NotificationCategoryManagerTest, TestNotificationExtensionNoButtons) {
  NSString* category_id = manager_->GetOrCreateCategory(
      "notification_id", /*buttons=*/{}, /*settings_button=*/false);
  ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
  UNNotificationCategory* category =
      [[fake_notification_center_ categories] anyObject];
  EXPECT_NSEQ(category_id, [category identifier]);

  // Test contents of the category
  EXPECT_EQ(0ul, [[category actions] count]);
}

TEST_F(NotificationCategoryManagerTest, TestNotificationExtensionTwoButtons) {
  NSString* category_id = manager_->GetOrCreateCategory(
      "notification_id", /*buttons=*/
      {{u"Button1", /*placeholder=*/std::nullopt},
       {u"Button2", /*placeholder=*/std::nullopt}},
      /*settings_button=*/false);
  ASSERT_EQ(1u, [[fake_notification_center_ categories] count]);
  UNNotificationCategory* category =
      [[fake_notification_center_ categories] anyObject];
  EXPECT_NSEQ(category_id, [category identifier]);

  // Test contents of the category
  EXPECT_EQ("Button1", base::SysNSStringToUTF8([[category actions][0] title]));
  EXPECT_EQ(base::SysNSStringToUTF8(kNotificationButtonOne),
            base::SysNSStringToUTF8([[category actions][0] identifier]));

  EXPECT_EQ(2ul, [[category actions] count]);

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

TEST_F(NotificationCategoryManagerTest, CreateTwoCategories) {
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

TEST_F(NotificationCategoryManagerTest, ReusesCategory) {
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

TEST_F(NotificationCategoryManagerTest, ReleaseMultipleCategories) {
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

TEST_F(NotificationCategoryManagerTest, ReleaseAllCategories) {
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

TEST_F(NotificationCategoryManagerTest, InitializeExistingCategories) {
  NotificationCategoryManager::CategoryKey category_key(
      {{{u"Action", u"Reply"}}, /*settings_button=*/true});
  UNNotificationCategory* category_ns =
      NotificationCategoryManager::CreateCategory(category_key);
  FakeUNNotification* notification =
      CreateNotification(@"identifier1", category_ns);
  auto* notification_ns = static_cast<UNNotification*>(notification);

  NSArray<UNNotification*>* notifications = @[ notification_ns ];
  NSSet<UNNotificationCategory*>* categories =
      [NSSet setWithArray:@[ category_ns ]];

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

}  // namespace mac_notifications
