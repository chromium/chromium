// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/page_break_app_item.h"

// static
const char PageBreakAppItem::kItemType[] = "DefaultPageBreak";

PageBreakAppItem::PageBreakAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& app_id)
    : ChromeAppListItem(profile, app_id) {
  SetIsPageBreak(true);

  if (sync_item) {
    DCHECK_EQ(sync_item->item_type, sync_pb::AppListSpecifics::TYPE_PAGE_BREAK);
    if (sync_item->item_ordinal.IsValid()) {
      UpdateFromSync(sync_item);
      return;
    }
  }

  SetDefaultPositionIfApplicable(model_updater);
}

PageBreakAppItem::~PageBreakAppItem() = default;

// ChromeAppListItem:
void PageBreakAppItem::Activate(int event_flags) {
  NOTREACHED();
}

const char* PageBreakAppItem::GetItemType() const {
  return kItemType;
}
