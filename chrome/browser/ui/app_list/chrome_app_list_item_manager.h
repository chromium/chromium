// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_MANAGER_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_MANAGER_H_

#include <map>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"

class ChromeAppListItem;

// The class to manage chrome app list items and never talks to Ash.
class ChromeAppListItemManager {
 public:
  ChromeAppListItemManager();

  ChromeAppListItemManager(const ChromeAppListItemManager&) = delete;
  ChromeAppListItemManager& operator=(const ChromeAppListItemManager&) = delete;

  ~ChromeAppListItemManager();

  // Methods to find/add/update/remove an item.
  ChromeAppListItem* FindItem(const std::string& id);
  ChromeAppListItem* AddChromeItem(std::unique_ptr<ChromeAppListItem> app_item);
  void UpdateChromeItem(const std::string& id,
                        std::unique_ptr<ash::AppListItemMetadata>);
  void RemoveChromeItem(const std::string& id);

  // Implement `ChromeAppListModelUpdater` methods.
  size_t ItemCount() const;
  int BadgedItemCount() const;
  std::vector<ChromeAppListItem*> GetTopLevelItems() const;

  // Returns a position that is greater than all valid positions in `items_`.
  syncer::StringOrdinal CreateChromePositionOnLast() const;

  const auto& items() const { return items_; }

 private:
  // A map from a ChromeAppListItem's id to its unique pointer. This item set
  // matches the one in AppListModel.
  std::map<std::string, std::unique_ptr<ChromeAppListItem>> items_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_MANAGER_H_
