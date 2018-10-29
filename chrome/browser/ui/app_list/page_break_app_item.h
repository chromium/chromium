// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_PAGE_BREAK_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_PAGE_BREAK_APP_ITEM_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

// Represents a page break item in the app list.
class PageBreakAppItem : public ChromeAppListItem {
 public:
  static const char kItemType[];

  // If a page break item with ID |app_id| exists in the local sync items as
  // |sync_item| (i.e. non-null), then this newly created item will be updated
  // from |sync_item|.
  PageBreakAppItem(Profile* profile,
                   AppListModelUpdater* model_updater,
                   const app_list::AppListSyncableService::SyncItem* sync_item,
                   const std::string& app_id);
  ~PageBreakAppItem() override;

  // ChromeAppListItem:
  void Activate(int event_flags) override;
  const char* GetItemType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageBreakAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_PAGE_BREAK_APP_ITEM_H_
