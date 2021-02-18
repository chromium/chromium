// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/notification_category_manager.h"

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

NotificationCategoryManager::NotificationCategoryManager(
    UNUserNotificationCenter* notification_center)
    : notification_center_([notification_center retain]) {}

NotificationCategoryManager::~NotificationCategoryManager() = default;

NSString* NotificationCategoryManager::GetOrCreateCategory(
    const std::string& notification_id,
    const Buttons& buttons,
    bool settings_button) {
  // Update category associations for the given |notification_id|.
  ReleaseCategory(notification_id);
  auto category_key = std::make_pair(buttons, settings_button);
  notification_id_buttons_map_.emplace(notification_id, category_key);

  // Try to find an existing category with the given buttons.
  auto existing = buttons_category_map_.find(category_key);
  if (existing != buttons_category_map_.end()) {
    UNNotificationCategory* category = existing->second.first.get();
    int& refcount = existing->second.second;
    // Increment refcount so we keep the category alive.
    ++refcount;
    return [category identifier];
  }

  // Create a new category with the given buttons.
  UNNotificationCategory* category = CreateCategory(buttons, settings_button);
  CategoryEntry category_entry([category retain], /*refcount=*/1);
  buttons_category_map_.emplace(category_key, std::move(category_entry));

  UpdateNotificationCenterCategories();

  return [category identifier];
}

void NotificationCategoryManager::ReleaseCategory(
    const std::string& notification_id) {
  auto existing_key = notification_id_buttons_map_.find(notification_id);
  if (existing_key == notification_id_buttons_map_.end())
    return;

  CategoryKey category_key = std::move(existing_key->second);
  notification_id_buttons_map_.erase(existing_key);

  auto existing_entry = buttons_category_map_.find(category_key);
  if (existing_entry == buttons_category_map_.end())
    return;

  // Decrement refcount and cleanup the category if unused.
  int& refcount = existing_entry->second.second;
  --refcount;
  if (refcount > 0)
    return;

  buttons_category_map_.erase(existing_entry);
  UpdateNotificationCenterCategories();
}

void NotificationCategoryManager::UpdateNotificationCenterCategories() {
  base::scoped_nsobject<NSMutableSet> categories([[NSMutableSet alloc] init]);
  for (const auto& entry : buttons_category_map_)
    [categories addObject:entry.second.first];

  [notification_center_ setNotificationCategories:categories];
}

UNNotificationCategory* NotificationCategoryManager::CreateCategory(
    const NotificationCategoryManager::Buttons& buttons,
    bool settings_button) {
  NSMutableArray* buttons_array = [NSMutableArray arrayWithCapacity:4];

  UNNotificationAction* close_button = [UNNotificationAction
      actionWithIdentifier:notification_constants::kNotificationCloseButtonTag
                     title:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_CLOSE)
                   options:UNNotificationActionOptionNone];

  // macOS 11 shows a close button in the top-left corner.
  if (!base::mac::IsAtLeastOS11())
    [buttons_array addObject:close_button];

  // We only support up to two user action buttons.
  DCHECK_LE(buttons.size(), 2u);
  if (buttons.size() >= 1u) {
    UNNotificationAction* button = [UNNotificationAction
        actionWithIdentifier:notification_constants::kNotificationButtonOne
                       title:base::SysUTF16ToNSString(buttons[0])
                     options:UNNotificationActionOptionNone];
    [buttons_array addObject:button];
  }
  if (buttons.size() >= 2u) {
    UNNotificationAction* button = [UNNotificationAction
        actionWithIdentifier:notification_constants::kNotificationButtonTwo
                       title:base::SysUTF16ToNSString(buttons[1])
                     options:UNNotificationActionOptionNone];
    [buttons_array addObject:button];
  }

  if (settings_button) {
    UNNotificationAction* button = [UNNotificationAction
        actionWithIdentifier:notification_constants::
                                 kNotificationSettingsButtonTag
                       title:l10n_util::GetNSString(
                                 IDS_NOTIFICATION_BUTTON_SETTINGS)
                     options:UNNotificationActionOptionNone];
    [buttons_array addObject:button];
  }

  // If there are only 2 buttons [Close, button] then the actions array needs to
  // be set as [button, Close] so that close is on top. If there are more than 2
  // buttons or we're on macOS 11, the buttons end up in an overflow menu which
  // shows them the correct way around.
  if (!base::mac::IsAtLeastOS11() && [buttons_array count] == 2) {
    // Remove the close button and move it to the end of the array.
    [buttons_array removeObject:close_button];
    [buttons_array addObject:close_button];
  }

  NSString* category_id = base::SysUTF8ToNSString(
      base::GUID::GenerateRandomV4().AsLowercaseString());

  UNNotificationCategory* category = [UNNotificationCategory
      categoryWithIdentifier:category_id
                     actions:buttons_array
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionCustomDismissAction];

  // This uses a private API to make sure the close button is always visible in
  // both alerts and banners, and modifies its content so that it is consistent
  // with the rest of the notification buttons. Otherwise, the text inside the
  // close button will come from the Apple API.
  if (!base::mac::IsAtLeastOS11() &&
      [category respondsToSelector:@selector(alternateAction)]) {
    [buttons_array removeObject:close_button];
    [category setValue:buttons_array forKey:@"actions"];
    [category setValue:close_button forKey:@"_alternateAction"];
  }

  // This uses a private API to change the text of the actions menu title so
  // that it is consistent with the rest of the notification buttons
  if ([category respondsToSelector:@selector(actionsMenuTitle)]) {
    [category setValue:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_MORE)
                forKey:@"_actionsMenuTitle"];
  }

  return category;
}
