// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "components/sync/model/string_ordinal.h"

namespace app_list {
namespace reorder {

// If the entropy (i.e. the ratio of the number of items out of order to the
// total number) is greater than this value, the sort order is reset to kCustom.
extern const float kOrderResetThreshold;

struct ReorderParam {
  ReorderParam(const std::string& new_sync_item_id,
               const syncer::StringOrdinal& new_ordinal);
  ReorderParam(const ReorderParam&);
  ~ReorderParam();

  // The sync item id.
  std::string sync_item_id;

  // The new ordinal for the sync item identified by `sync_item_id`.
  syncer::StringOrdinal ordinal;
};

// Wrapping a sync item for comparison.
template <typename T>
struct SyncItemWrapper {
  explicit SyncItemWrapper(const AppListSyncableService::SyncItem& sync_item);
  explicit SyncItemWrapper(const ChromeAppListItem& app_list_item);

  std::string id;
  syncer::StringOrdinal item_ordinal;
  bool is_folder = false;

  // The attribute for comparison.
  T key_attribute;
};

// SyncItemWrapper<std::string> ------------------------------------------------

// Full template specialization to support name ordering.
template <>
SyncItemWrapper<std::string>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item);
template <>
SyncItemWrapper<std::string>::SyncItemWrapper(
    const ChromeAppListItem& app_list_item);

// Overloaded comparison operators:
bool operator<(const SyncItemWrapper<std::string>& lhs,
               const SyncItemWrapper<std::string>& rhs);
bool operator>(const SyncItemWrapper<std::string>& lhs,
               const SyncItemWrapper<std::string>& rhs);

// Gets a list of string wrappers based on the mappings from ids to sync items.
std::vector<SyncItemWrapper<std::string>> GenerateStringWrappersFromSyncItems(
    const AppListSyncableService::SyncItemMap& sync_item_map);

// Gets a list of string wrappers based on the given app list items.
std::vector<SyncItemWrapper<std::string>>
GenerateStringWrappersFromAppListItems(
    const std::vector<const ChromeAppListItem*>& app_list_items);

}  // namespace reorder
}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_
