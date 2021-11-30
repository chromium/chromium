// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_

#include "components/sync/model/string_ordinal.h"

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

  // Returns the sorting order that is saved in perf service and gets shared
  // among synced devices.
  virtual ash::AppListSortOrder GetPermanentSortingOrder() const = 0;
};

}  // namespace reorder
}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
