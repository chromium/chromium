// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <string>

namespace app_list {

// Interface for a ranker.
//
// It's recommended to keep as much Finch experiment logic out of subclasses as
// possible. Instead, favor creating new rankers and conditionally adding them
// in SearchController::InitializeRankers.
class Ranker {
 public:
  Ranker() {}
  virtual ~Ranker() {}

  Ranker(const Ranker&) = delete;
  Ranker& operator=(const Ranker&) = delete;

  // Called each time a new search session begins, eg. when the user types a
  // character.
  virtual void Start(const std::u16string& query,
                     ResultsMap& results,
                     CategoriesMap& categories) {}

  // Called each time a search provider sets new results. Passed the |provider|
  // type that triggered this call, and all |results| received so far for this
  // search session.
  //
  // The results for a provider can be updated more than once in a search
  // session, which will invalidate pointers to previous results. It is
  // recommended that rankers don't explicitly store any result pointers.
  //
  // The goal of a ranker should be to update scores in the Scoring structs
  // within |results|. Generally, one ranker should map to one score member.
  virtual void Rank(ResultsMap& results,
                    CategoriesMap& categories,
                    ProviderType provider) {}

  // Called each time a user launches a result.
  virtual void Train(const LaunchData& launch) {}

  // Called each time a user removes a result from the search results list.
  virtual void Remove(ChromeSearchResult* result) {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_H_
