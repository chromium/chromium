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
class PrefService;

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
      const std::vector<const ChromeAppListItem*>& app_list_items);

  // Calculates the position for an incoming item based on the specified sorting
  // order and pass results through parameters. `local_items` indicates the
  // elements in the active app list model before adding the new item. Note that
  // different devices may have different sets of app list items. It is why the
  // parameter is named `local_items`. `order_ignored` is true if it is not
  // appropriate to place `new_item` following `order`. It is because the
  // arrangement of `local_items` does not follow `order`. In this case, the new
  // item should be placed at the front.
  void CalculateNewItemPosition(
      ash::AppListSortOrder order,
      const ChromeAppListItem& new_item,
      const std::vector<const ChromeAppListItem*>& local_items,
      syncer::StringOrdinal* target_position,
      bool* order_ignored) const;

 private:
  // Returns the foremost item position across syncable devices.
  syncer::StringOrdinal CalculateFrontPosition() const;

  // Calculates the position for an incoming item based on the namer order that
  // is either kNameAlphabetical or kNameReverseAlphabetical. Pass results
  // through parameters. Read the comment of `CalculateNewItemPosition()` for
  // the remaining parameters' meaning.
  // TODO(https://crbug.com/1261899): the function name is misleading. Actually
  // this function is not const because the sort order saved in prefs could be
  // reset. Replace with a better name.
  void CalculatePositionInNameOrder(
      ash::AppListSortOrder order,
      const ChromeAppListItem& new_item,
      const std::vector<const ChromeAppListItem*>& local_items,
      syncer::StringOrdinal* target_position,
      bool* order_ignored) const;

  PrefService* GetPrefService();

  AppListSyncableService* const app_list_syncable_service_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
