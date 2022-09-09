// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_LAUNCH_DATA_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_LAUNCH_DATA_H_

#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/search/ranking/ranking_item_util.h"

namespace app_list {

// Data concerning the app launch. Used for training models.
struct LaunchData {
  LaunchData();
  ~LaunchData();

  LaunchData(LaunchData&& other);
  // ID of the item launched.
  std::string id;
  // The type of the result.
  ash::AppListSearchResultType result_type =
      ash::AppListSearchResultType::kUnknown;
  // The type of the result used for ranking.
  // TODO(crbug.com/1199206): This is no longer needed and can be removed when
  // the search_result_ranker/ directory is removed.
  RankingItemType ranking_item_type = RankingItemType::kUnknown;
  // TODO(crbug.com/1258415): Deprecate and can be removed along with app launch
  // logging. Where the app was launched from.
  ash::AppListLaunchedFrom launched_from =
      ash::AppListLaunchedFrom::kLaunchedFromShelf;
  // The type of app launched.
  ash::AppListLaunchType launch_type = ash::AppListLaunchType::kSearchResult;
  // The index of the suggested app launched, if applicable.
  int suggestion_index = 0;
  // User's search query string, if relevant.
  std::string query;
  // The score of the suggestion.
  double score = 0.0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_LAUNCH_DATA_H_
