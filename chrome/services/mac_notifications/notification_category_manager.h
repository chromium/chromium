// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_NOTIFICATION_CATEGORY_MANAGER_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_NOTIFICATION_CATEGORY_MANAGER_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"

namespace mac_notifications {

// This class manages notification categories for a given NotificationCenter.
// Notification categories on macOS describe all action buttons that should be
// shown on a notification. Each NotificationCenter has a global set of
// categories that can be used on notifications shown with it. This class
// manages that set and returns category identifiers for a given set of action
// buttons.
class NotificationCategoryManager {
 public:
  using Button = std::pair</*title*/ std::u16string,
                           /*placeholder*/ std::optional<std::u16string>>;
  using Buttons = std::vector<Button>;

  explicit NotificationCategoryManager(
      UNUserNotificationCenter* notification_center);
  NotificationCategoryManager(const NotificationCategoryManager&) = delete;
  NotificationCategoryManager& operator=(const NotificationCategoryManager&) =
      delete;
  ~NotificationCategoryManager();

  // Initializes notification categories from displayed notifications.
  void InitializeExistingCategories(NSArray<UNNotification*>* notifications,
                                    NSSet<UNNotificationCategory*>* categories);

  // Gets an existing category identifier that matches the given action buttons
  // or creates a new one. The returned identifier will stay valid until all
  // notifications using that category have been closed.
  NSString* GetOrCreateCategory(const std::string& notification_id,
                                const Buttons& buttons,
                                bool settings_button);

  // Releases the category used for |notification_ids|. This needs to be called
  // once the notifications have been closed to clean up unused categories.
  void ReleaseCategories(const std::vector<std::string>& notification_ids);

  // Releases all categories managed for |notification_center_|.
  void ReleaseAllCategories();

 private:
  FRIEND_TEST_ALL_PREFIXES(MacNotificationServiceUNTest,
                           InitializeDeliveredNotifications);
  FRIEND_TEST_ALL_PREFIXES(NotificationCategoryManagerTest,
                           InitializeExistingCategories);

  // Synchronizes the set of currently used notification categories with the
  // system NotificationCenter.
  void UpdateNotificationCenterCategories();

  using CategoryKey = std::pair<Buttons, /*settings_button*/ bool>;
  using CategoryEntry = std::pair<UNNotificationCategory*,
                                  /*refcount*/ int>;

  // Creates a new category that shows the buttons given via |key|.
  static UNNotificationCategory* CreateCategory(const CategoryKey& key);

  // Gets the CategoryKey used to create |category|.
  static CategoryKey GetCategoryKey(UNNotificationCategory* category);

  // Maps the set of notification action buttons to a category.
  base::flat_map<CategoryKey, CategoryEntry> buttons_category_map_;

  // Maps a notification id to its set of notification action buttons.
  base::flat_map<std::string, CategoryKey> notification_id_buttons_map_;

  UNUserNotificationCenter* __strong notification_center_;
};

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_NOTIFICATION_CATEGORY_MANAGER_H_
