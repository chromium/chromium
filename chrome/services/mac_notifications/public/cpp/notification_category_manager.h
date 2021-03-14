// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_CATEGORY_MANAGER_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_CATEGORY_MANAGER_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/mac/scoped_nsobject.h"

// This class manages notification categories for a given NotificationCenter.
// Notification categories on macOS describe all action buttons that should be
// shown on a notification. Each NotificationCenter has a global set of
// categories that can be used on notifications shown with it. This class
// manages that set and returns category identifiers for a given set of action
// buttons.
class API_AVAILABLE(macos(10.14)) NotificationCategoryManager {
 public:
  using Buttons = std::vector<std::u16string>;

  explicit NotificationCategoryManager(
      UNUserNotificationCenter* notification_center);
  NotificationCategoryManager(const NotificationCategoryManager&) = delete;
  NotificationCategoryManager& operator=(const NotificationCategoryManager&) =
      delete;
  ~NotificationCategoryManager();

  // Gets an existing category identifier that matches the given action buttons
  // or creates a new one. The returned identifier will stay valid until all
  // notifications using that category have been closed.
  NSString* GetOrCreateCategory(const std::string& notification_id,
                                const Buttons& buttons,
                                bool settings_button);

  // Releases the category used for |notification_id|. This needs to be called
  // once the notification has been closed to clean up unused categories.
  void ReleaseCategory(const std::string& notification_id);

 private:
  // Synchronizes the set of currently used notification categories with the
  // system NotificationCenter.
  void UpdateNotificationCenterCategories();

  // Creates a new notification category that shows the given |buttons|.
  UNNotificationCategory* CreateCategory(const Buttons& buttons,
                                         bool settings_button);

  using CategoryKey = std::pair<Buttons, /*settings_button*/ bool>;
  using CategoryEntry = std::pair<base::scoped_nsobject<UNNotificationCategory>,
                                  /*refcount*/ int>;

  // Maps the set of notification action buttons to a category.
  base::flat_map<CategoryKey, CategoryEntry> buttons_category_map_;

  // Maps a notification id to its set of notification action buttons.
  base::flat_map<std::string, CategoryKey> notification_id_buttons_map_;

  base::scoped_nsobject<UNUserNotificationCenter> notification_center_;
};

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_CATEGORY_MANAGER_H_
