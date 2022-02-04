// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_

#include "components/sync/model/string_ordinal.h"

class ChromeAppListItem;

namespace ash {
enum class AppListSortOrder;
}

namespace app_list {
namespace reorder {

// An interface to provide functions for app list sorting.
class AppListReorderDelegate {
 public:
  virtual ~AppListReorderDelegate() {}

  // Sets the preferred sorting order.
  virtual void SetAppListPreferredOrder(ash::AppListSortOrder order) = 0;

  // Returns the front position among all sync items.
  virtual syncer::StringOrdinal CalculateGlobalFrontPosition() const = 0;

  // Calcuates a position for a `new_item` so the preferred sort order is
  // preserved. The sort order is returned through `target_position`. Returns
  // whether the item should be placed in the sort order, in which case
  // `target_position` gets set.
  virtual bool CalculateNewItemPosition(
      const ChromeAppListItem& new_item,
      const std::vector<const ChromeAppListItem*>& local_items,
      syncer::StringOrdinal* target_position) const = 0;

  // Returns the sorting order that is saved in perf service and gets shared
  // among synced devices.
  virtual ash::AppListSortOrder GetPermanentSortingOrder() const = 0;
};

}  // namespace reorder
}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
