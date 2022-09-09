// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"

class ChromeSearchResult;

namespace ash {
enum class AppListSearchResultType;
}

namespace base {
class Time;
class TimeDelta;
}

namespace app_list {

class SearchProvider;
enum class RankingItemType;

// Common types used throughout result ranking.

using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;
using ResultsMap = base::flat_map<ProviderType, Results>;

// Controller that collects query from given SearchBoxModel, dispatches it
// to all search providers, then invokes the mixer to mix and to publish the
// results to the given SearchResults UI model.
//
// TODO(crbug.com/1199206): The SearchController is being reimplemented with
// a different ranking system. Once this reimplementation is finished, this pure
// virtual class can be removed and replaced with SearchControllerImplNew.
class SearchController {
 public:
  using ResultsChangedCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType)>;

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever results are added to the launcher, as a result of
    // zero-state or from a user query. This will be called multiple times per
    // query because launcher results arrive incrementally.
    //
    // Observers should not store the ChromeSearchResult* pointers or post them
    // to another sequence because they may be invalidated.
    virtual void OnResultsAdded(
        const std::u16string& query,
        const std::vector<const ChromeSearchResult*>& results) {}

    // Called whenever old results are cleared. This occurs whenever a new
    // search is started.
    virtual void OnResultsCleared() {}
  };

  virtual ~SearchController() {}

  virtual void InitializeRankers() {}

  virtual void StartSearch(const std::u16string& query) = 0;
  virtual void StartZeroState(base::OnceClosure on_done,
                              base::TimeDelta timeout) = 0;

  virtual void AppListClosing() = 0;

  virtual void OpenResult(ChromeSearchResult* result, int event_flags) = 0;
  virtual void InvokeResultAction(ChromeSearchResult* result,
                                  ash::SearchResultActionType action) = 0;

  // Adds a new mixer group. See Mixer::AddGroup.
  virtual size_t AddGroup(size_t max_results) = 0;

  // Takes ownership of |provider| and associates it with given mixer group.
  virtual void AddProvider(size_t group_id,
                           std::unique_ptr<SearchProvider> provider) = 0;

  // Update the controller with the given results. Used only if the categorical
  // search feature flag is enabled.
  virtual void SetResults(const SearchProvider* provider, Results results) = 0;
  // Publishes results to ash.
  virtual void Publish() = 0;

  virtual ChromeSearchResult* FindSearchResult(
      const std::string& result_id) = 0;
  virtual ChromeSearchResult* GetResultByTitleForTest(
      const std::string& title) = 0;

  // Sends training signal to each |providers_|
  virtual void Train(LaunchData&& launch_data) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual std::u16string get_query() = 0;

  virtual base::Time session_start() = 0;

  virtual void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) = 0;

  virtual void disable_ranking_for_test() = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
