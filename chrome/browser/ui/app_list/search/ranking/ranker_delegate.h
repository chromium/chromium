// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_DELEGATE_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"

class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace app_list {

class SearchController;

// TODO(crbug.com/1199206): Implement.
class RankerDelegate : Ranker {
 public:
  RankerDelegate(Profile* profile,
                 AppListModelUpdater* model_updater,
                 SearchController* controller);
  ~RankerDelegate();
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_DELEGATE_H_
