// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace {

int g_next_unique_model_id = ash::kAppListProfileIdStartFrom;

}  // namespace

AppListModelUpdater::AppListModelUpdater()
    : model_id_(g_next_unique_model_id++) {}

// static
syncer::StringOrdinal AppListModelUpdater::GetFirstAvailablePositionInternal(
    const std::vector<ChromeAppListItem*>& top_level_items) {
  // Sort the top level items by their positions.
  std::vector<ChromeAppListItem*> sorted_items(top_level_items);
  std::sort(sorted_items.begin(), sorted_items.end(),
            [](ChromeAppListItem* const& item1,
               ChromeAppListItem* const& item2) -> bool {
              return item1->position().LessThan(item2->position());
            });

  // Find the first empty position in app list. If all pages are full, return
  // the next position after last item.
  int items_in_page = 0;
  int page = 0;
  for (size_t i = 0; i < sorted_items.size(); ++i) {
    if (!sorted_items[i]->is_page_break()) {
      ++items_in_page;
      continue;
    }

    // There may be multiple "page break" items at the end of page while empty
    // pages will not be shown in app list, so skip them.
    const int max_items_in_page =
        ash::AppListConfig::instance().GetMaxNumOfItemsPerPage(page);
    if (items_in_page > 0 && items_in_page < max_items_in_page) {
      // Sometimes two continuous items may have the same position, so skip to
      // the next available position.
      // |i| should always be larger than 0 here because |items_in_page| is
      // larger than 0.
      if (sorted_items[i - 1]->position().LessThan(
              sorted_items[i]->position())) {
        return sorted_items[i - 1]->position().CreateBetween(
            sorted_items[i]->position());
      }
    }
    if (items_in_page > 0)
      ++page;
    items_in_page = 0;
  }

  if (sorted_items.empty())
    return syncer::StringOrdinal::CreateInitialOrdinal();
  return sorted_items.back()->position().CreateAfter();
}
