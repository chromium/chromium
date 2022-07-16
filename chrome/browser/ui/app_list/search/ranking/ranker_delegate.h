// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_DELEGATE_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"

class ChromeSearchResult;
class Profile;

namespace app_list {

class SearchController;

// A delegate for a series of rankers. Rankers can be added via AddRanker, and
// all other methods will delegate the call to each ranker in the order they
// were added.
class RankerDelegate : public Ranker {
 public:
  RankerDelegate(Profile* profile,
                 SearchController* controller);
  ~RankerDelegate() override;

  RankerDelegate(const RankerDelegate&) = delete;
  RankerDelegate& operator=(const RankerDelegate&) = delete;

  void AddRanker(std::unique_ptr<Ranker> ranker);

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesMap& categories) override;
  void Rank(ResultsMap& results,
            CategoriesMap& categories,
            ProviderType provider) override;
  void Train(const LaunchData& launch) override;
  void Remove(ChromeSearchResult* result) override;

 private:
  std::vector<std::unique_ptr<Ranker>> rankers_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_DELEGATE_H_
