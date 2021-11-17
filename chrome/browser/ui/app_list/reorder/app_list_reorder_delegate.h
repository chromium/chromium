// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_

#include <stack>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/reorder/app_list_reorder_util.h"
#include "components/sync/model/string_ordinal.h"

class ChromeAppListItem;

namespace app_list {
namespace reorder {
struct ReorderParam;
}

class AppListSyncableService;

// The helper class for sorting launcher apps in order.
class AppListReorderDelegate {
 public:
  class TestApi {
   public:
    explicit TestApi(AppListReorderDelegate* reorder_delegate);

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    ~TestApi();

    // Returns the entropy (i.e. the ratio of the out-of-order item number to
    // the total number) based on the specified order.
    float CalculateEntropy(ash::AppListSortOrder order) const;

   private:
    AppListReorderDelegate* const reorder_delegate_;
  };

  explicit AppListReorderDelegate(
      AppListSyncableService* app_list_syncable_service);
  AppListReorderDelegate(const AppListReorderDelegate&) = delete;
  AppListReorderDelegate& operator=(const AppListReorderDelegate&) = delete;
  ~AppListReorderDelegate();

  // Returns reorder params which indicate the changes in ordinals to ensure
  // `order` among the specified sync items. This function ensures that the
  // number of updates is as few as possible.
  std::vector<reorder::ReorderParam> GenerateReorderParamsForSyncItems(
      ash::AppListSortOrder order,
      const AppListSyncableService::SyncItemMap& sync_item_map) const;

  // Similar to `GenerateReorderParamsForSyncItems()` but accepts a vector of
  // `ChromeAppListItem` pointers as the parameter.
  std::vector<reorder::ReorderParam> GenerateReorderParamsForAppListItems(
      ash::AppListSortOrder order,
      const std::vector<const ChromeAppListItem*>& app_list_items) const;

  // Calculates the position for an incoming item based on the specified sorting
  // order and pass the target position through the parameter. Returns whether
  // `target_position` is set. The return value is false if it is not propriate
  // to place `new_item` following `order`. In this case, `target_position` is
  // not set.
  // `local_items` indicates the elements in the active app list model before
  // adding the new item. Note that different devices may have different sets of
  // app list items. It is why the parameter is named `local_items`.
  // `global_items` is the sync data of the items across synced devices. If
  // `global_items` is null, position is calculated only based on `local_items`.
  bool CalculateNewItemPosition(
      ash::AppListSortOrder order,
      const ChromeAppListItem& new_item,
      const std::vector<const ChromeAppListItem*>& local_items,
      const AppListSyncableService::SyncItemMap* global_items,
      syncer::StringOrdinal* target_position) const;

  // Returns the foremost item position across syncable devices.
  syncer::StringOrdinal CalculateFrontPosition(
      const AppListSyncableService::SyncItemMap& sync_item_map) const;

 private:
  // Similar to `CalculateNewItemPosition()` but `order` is either
  // kNameAlphabetical or kNameReverseAlphabetical. Read the comment of
  // `CalculateNewItemPosition()` for parameters' meanings.
  bool CalculatePositionInNameOrder(
      ash::AppListSortOrder order,
      const ChromeAppListItem& new_item,
      const std::vector<const ChromeAppListItem*>& local_items,
      const AppListSyncableService::SyncItemMap* global_items,
      syncer::StringOrdinal* target_position) const;

  AppListSyncableService* const app_list_syncable_service_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
