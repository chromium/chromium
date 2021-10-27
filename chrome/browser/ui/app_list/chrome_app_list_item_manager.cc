// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_app_list_item_manager.h"

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

ChromeAppListItemManager::ChromeAppListItemManager() = default;

ChromeAppListItemManager::~ChromeAppListItemManager() = default;

ChromeAppListItem* ChromeAppListItemManager::FindItem(const std::string& id) {
  auto iter = items_.find(id);
  return iter != items_.end() ? iter->second.get() : nullptr;
}

ChromeAppListItem* ChromeAppListItemManager::AddChromeItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  ChromeAppListItem* item = app_item.get();
  items_[app_item->id()] = std::move(app_item);
  return item;
}

void ChromeAppListItemManager::UpdateChromeItem(
    const std::string& id,
    std::unique_ptr<ash::AppListItemMetadata> data) {
  FindItem(id)->SetMetadata(std::move(data));
}

void ChromeAppListItemManager::RemoveChromeItem(const std::string& id) {
  items_.erase(id);
}

size_t ChromeAppListItemManager::ItemCount() const {
  return items_.size();
}

int ChromeAppListItemManager::BadgedItemCount() const {
  size_t count = 0u;
  for (const auto& key_val : items_) {
    if (key_val.second->IsBadged())
      ++count;
  }
  return count;
}

std::vector<ChromeAppListItem*> ChromeAppListItemManager::GetTopLevelItems()
    const {
  std::vector<ChromeAppListItem*> top_level_items;
  for (auto& entry : items_) {
    ChromeAppListItem* item = entry.second.get();
    DCHECK(item->position().IsValid())
        << "Item with invalid position: id=" << item->id()
        << ", name=" << item->name() << ", is_folder=" << item->is_folder()
        << ", is_page_break=" << item->is_page_break();
    if (item->folder_id().empty() && item->position().IsValid())
      top_level_items.emplace_back(item);
  }
  return top_level_items;
}

syncer::StringOrdinal ChromeAppListItemManager::CreateChromePositionOnLast()
    const {
  syncer::StringOrdinal last_known_position;
  for (auto& it : items_) {
    if (!last_known_position.IsValid() ||
        (it.second->position().IsValid() &&
         it.second->position().GreaterThan(last_known_position))) {
      last_known_position = it.second->position();
    }
  }
  return last_known_position.IsValid()
             ? last_known_position.CreateAfter()
             : syncer::StringOrdinal::CreateInitialOrdinal();
}
