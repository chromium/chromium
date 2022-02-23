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
//
// This is the place to configure experiments or flags that change ranking
// behavior.
//
// TODO(crbug.com/1199206): This is now much more than a delegate, because it
// does all the work of setting up the rankers. Rename to something more
// appropriate.
class RankerDelegate : public Ranker {
 public:
  RankerDelegate(Profile* profile, SearchController* controller);
  ~RankerDelegate() override;

  RankerDelegate(const RankerDelegate&) = delete;
  RankerDelegate& operator=(const RankerDelegate&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;
  void Train(const LaunchData& launch) override;
  void Remove(ChromeSearchResult* result) override;
  void OnBurnInPeriodElapsed() override;

 private:
  void AddRanker(std::unique_ptr<Ranker> ranker);

  std::vector<std::unique_ptr<Ranker>> rankers_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_DELEGATE_H_
