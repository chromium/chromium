// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_DATA_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_DATA_H_

#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

namespace app_list {

// Data concerning the app launch. Used for training models.
struct AppLaunchData {
  AppLaunchData();
  ~AppLaunchData();

  AppLaunchData(AppLaunchData&& other);
  // ID of the item launched.
  std::string id;
  RankingItemType ranking_item_type = RankingItemType::kUnknown;
  // Where the app was launched from.
  ash::AppListLaunchedFrom launched_from =
      ash::AppListLaunchedFrom::kLaunchedFromShelf;
  // The type of app launched.
  ash::AppListLaunchType launch_type = ash::AppListLaunchType::kSearchResult;
  // The index of the suggested app launched, if applicable.
  int suggestion_index = 0;
  // User's search query string, if relevant.
  std::string query;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_DATA_H_
