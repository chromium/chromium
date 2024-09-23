// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/notification_category_manager.h"

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/uuid.h"
#include "chrome/grit/generated_resources.h"
#import "chrome/services/mac_notifications/mac_notification_service_utils.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace mac_notifications {

namespace {

UNNotificationAction* CreateAction(
    const NotificationCategoryManager::Button& button,
    NSString* identifier) {
  if (button.second) {
    return [UNTextInputNotificationAction
        actionWithIdentifier:identifier
                       title:base::SysUTF16ToNSString(button.first)
                     options:UNNotificationActionOptionNone
        textInputButtonTitle:base::SysUTF16ToNSString(button.first)
        textInputPlaceholder:base::SysUTF16ToNSString(*button.second)];
  }

  return [UNNotificationAction
      actionWithIdentifier:identifier
                     title:base::SysUTF16ToNSString(button.first)
                   options:UNNotificationActionOptionNone];
}

NotificationCategoryManager::Button GetButtonFromAction(
    UNNotificationAction* action) {
  std::u16string title = base::SysNSStringToUTF16([action title]);
  std::optional<std::u16string> placeholder;

  if ([action isKindOfClass:[UNTextInputNotificationAction class]]) {
    auto* text_action = static_cast<UNTextInputNotificationAction*>(action);
    placeholder = base::SysNSStringToUTF16([text_action textInputPlaceholder]);
  }

  return {title, placeholder};
}

}  // namespace

NotificationCategoryManager::NotificationCategoryManager(
    UNUserNotificationCenter* notification_center)
    : notification_center_(notification_center) {}

NotificationCategoryManager::~NotificationCategoryManager() = default;

void NotificationCategoryManager::InitializeExistingCategories(
    NSArray<UNNotification*>* notifications,
    NSSet<UNNotificationCategory*>* categories) {
  base::flat_map<std::string, UNNotificationCategory*> category_map;

  // Setup map from category ID to category for faster lookup.
  for (UNNotificationCategory* category in categories) {
    std::string category_id = base::SysNSStringToUTF8(category.identifier);
    category_map.emplace(category_id, category);
  }

  // Setup links from notifications to categories and count how many times each
  // category is used.
  for (UNNotification* notification in notifications) {
    std::string notification_id =
        base::SysNSStringToUTF8([[notification request] identifier]);
    std::string category_id = base::SysNSStringToUTF8(
        notification.request.content.categoryIdentifier);

    if (notification_id_buttons_map_.count(notification_id))
      continue;

    auto entry = category_map.find(category_id);
    if (entry == category_map.end())
      continue;

    // Link |notification_id| to |category_key|.
    auto category_key = GetCategoryKey(entry->second);
    notification_id_buttons_map_.emplace(notification_id, category_key);

    // Increment refcount for |category_key|.
    auto existing = buttons_category_map_.find(category_key);
    if (existing != buttons_category_map_.end()) {
      ++existing->second.second;
    } else {
      CategoryEntry category_entry(entry->second, /*refcount=*/1);
      buttons_category_map_.emplace(category_key, std::move(category_entry));
    }
  }
}

NSString* NotificationCategoryManager::GetOrCreateCategory(
    const std::string& notification_id,
    const Buttons& buttons,
    bool settings_button) {
  // Update category associations for the given |notification_id|.
  ReleaseCategories({notification_id});
  auto category_key = std::make_pair(buttons, settings_button);
  notification_id_buttons_map_.emplace(notification_id, category_key);

  // Try to find an existing category with the given buttons.
  auto existing = buttons_category_map_.find(category_key);
  if (existing != buttons_category_map_.end()) {
    UNNotificationCategory* category = existing->second.first;
    int& refcount = existing->second.second;
    // Increment refcount so we keep the category alive.
    ++refcount;
    return [category identifier];
  }

  // Create a new category with the given buttons.
  UNNotificationCategory* category = CreateCategory(category_key);
  CategoryEntry category_entry(category, /*refcount=*/1);
  buttons_category_map_.emplace(category_key, std::move(category_entry));

  UpdateNotificationCenterCategories();

  return category.identifier;
}

void NotificationCategoryManager::ReleaseCategories(
    const std::vector<std::string>& notification_ids) {
  bool needs_update = false;

  for (const auto& notification_id : notification_ids) {
    auto existing_key = notification_id_buttons_map_.find(notification_id);
    if (existing_key == notification_id_buttons_map_.end())
      continue;

    CategoryKey category_key = std::move(existing_key->second);
    notification_id_buttons_map_.erase(existing_key);

    auto existing_entry = buttons_category_map_.find(category_key);
    if (existing_entry == buttons_category_map_.end())
      continue;

    // Decrement refcount and cleanup the category if unused.
    int& refcount = existing_entry->second.second;
    --refcount;
    if (refcount > 0)
      continue;

    buttons_category_map_.erase(existing_entry);
    needs_update = true;
  }

  if (needs_update)
    UpdateNotificationCenterCategories();
}

void NotificationCategoryManager::ReleaseAllCategories() {
  if (buttons_category_map_.empty() && notification_id_buttons_map_.empty())
    return;

  buttons_category_map_.clear();
  notification_id_buttons_map_.clear();
  UpdateNotificationCenterCategories();
}

void NotificationCategoryManager::UpdateNotificationCenterCategories() {
  NSMutableSet* categories = [[NSMutableSet alloc] init];
  for (const auto& entry : buttons_category_map_)
    [categories addObject:entry.second.first];

  [notification_center_ setNotificationCategories:categories];
}

UNNotificationCategory* NotificationCategoryManager::CreateCategory(
    const CategoryKey& key) {
  const NotificationCategoryManager::Buttons& buttons = key.first;
  bool settings_button = key.second;
  NSMutableArray* buttons_array = [NSMutableArray arrayWithCapacity:4];

  // We only support up to two user action buttons.
  DCHECK_LE(buttons.size(), 2u);
  if (buttons.size() >= 1u)
    [buttons_array addObject:CreateAction(buttons[0], kNotificationButtonOne)];
  if (buttons.size() >= 2u)
    [buttons_array addObject:CreateAction(buttons[1], kNotificationButtonTwo)];

  if (settings_button) {
    UNNotificationAction* button = [UNNotificationAction
        actionWithIdentifier:kNotificationSettingsButtonTag
                       title:l10n_util::GetNSString(
                                 IDS_NOTIFICATION_BUTTON_SETTINGS)
                     options:UNNotificationActionOptionNone];
    [buttons_array addObject:button];
  }

  NSString* category_id = base::SysUTF8ToNSString(
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  UNNotificationCategory* category = [UNNotificationCategory
      categoryWithIdentifier:category_id
                     actions:buttons_array
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionCustomDismissAction];

  // This uses a private API to change the text of the actions menu title so
  // that it is consistent with the rest of the notification buttons
  if ([category respondsToSelector:@selector(actionsMenuTitle)]) {
    [category setValue:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_MORE)
                forKey:@"_actionsMenuTitle"];
  }

  return category;
}

NotificationCategoryManager::CategoryKey
NotificationCategoryManager::GetCategoryKey(UNNotificationCategory* category) {
  Buttons buttons;
  bool settings_button = false;

  for (UNNotificationAction* action in category.actions) {
    NSString* identifier = action.identifier;
    if ([kNotificationSettingsButtonTag isEqualToString:identifier]) {
      settings_button = true;
    } else if ([kNotificationButtonOne isEqualToString:identifier] ||
               [kNotificationButtonTwo isEqualToString:identifier]) {
      buttons.push_back(GetButtonFromAction(action));
    }
  }

  return std::make_pair(std::move(buttons), settings_button);
}

}  // namespace mac_notifications
