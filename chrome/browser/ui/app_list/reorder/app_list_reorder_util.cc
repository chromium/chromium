// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/reorder/app_list_reorder_util.h"

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace app_list {
namespace reorder {

// ReorderParam ----------------------------------------------------------------

ReorderParam::ReorderParam(const std::string& new_sync_item_id,
                           const syncer::StringOrdinal& new_ordinal)
    : sync_item_id(new_sync_item_id), ordinal(new_ordinal) {}

ReorderParam::ReorderParam(const ReorderParam&) = default;

ReorderParam::~ReorderParam() = default;

// Method Implementations -----------------------------------------------------

std::vector<SyncItemWrapper<std::string>> GenerateStringWrappersFromSyncItems(
    const AppListSyncableService::SyncItemMap& sync_item_map) {
  std::vector<SyncItemWrapper<std::string>> wrappers;
  for (const auto& id_item_pair : sync_item_map) {
    auto* sync_item = id_item_pair.second.get();

    if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK)
      continue;

    SyncItemWrapper<std::string> wrapper;
    wrapper.id = sync_item->item_id;
    wrapper.item_ordinal = sync_item->item_ordinal;
    wrapper.key_attribute = sync_item->item_name;
    wrapper.is_folder =
        sync_item->item_type == sync_pb::AppListSpecifics::TYPE_FOLDER;
    wrappers.emplace_back(std::move(wrapper));
  }

  return wrappers;
}

}  // namespace reorder
}  // namespace app_list
