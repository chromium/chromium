// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_

#include <stack>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "components/sync/model/string_ordinal.h"

namespace app_list {
namespace reorder {
struct ReorderParam;
}

class AppListSyncableService;

// The helper class for sorting launcher apps in order.
class AppListReorderDelegate {
 public:
  explicit AppListReorderDelegate(
      AppListSyncableService* app_list_syncable_service);
  AppListReorderDelegate(const AppListReorderDelegate&) = delete;
  AppListReorderDelegate& operator=(const AppListReorderDelegate&) = delete;
  ~AppListReorderDelegate() = default;

  // Returns reorder params which indicate the changes in ordinals to ensure
  // `order` among sync items. This function ensures that the number of updates
  // is as few as possible.
  std::vector<reorder::ReorderParam> GenerateReorderParams(
      ash::AppListSortOrder order) const;

 private:
  AppListSyncableService* const app_list_syncable_service_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
