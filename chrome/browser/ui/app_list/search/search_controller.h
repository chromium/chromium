// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace app_list {

class SearchResultRanker;
class SearchProvider;
enum class RankingItemType;

// Controller that collects query from given SearchBoxModel, dispatches it
// to all search providers, then invokes the mixer to mix and to publish the
// results to the given SearchResults UI model.
class SearchController {
 public:
  SearchController(AppListModelUpdater* model_updater,
                   AppListControllerDelegate* list_controller,
                   Profile* profile);
  virtual ~SearchController();

  void InitializeRankers();

  void Start(const base::string16& query);
  void ViewClosing();

  void OpenResult(ChromeSearchResult* result, int event_flags);
  void InvokeResultAction(ChromeSearchResult* result,
                          int action_index,
                          int event_flags);

  // Adds a new mixer group. See Mixer::AddGroup.
  size_t AddGroup(size_t max_results, double multiplier, double boost);

  // Takes ownership of |provider| and associates it with given mixer group.
  void AddProvider(size_t group_id, std::unique_ptr<SearchProvider> provider);

  virtual ChromeSearchResult* FindSearchResult(const std::string& result_id);
  ChromeSearchResult* GetResultByTitleForTest(const std::string& title);

  // Sends training signal to each |providers_|
  void Train(AppLaunchData&& app_launch_data);

  // Invoked when the app list is shown.
  void AppListShown();

  // Gets the search result ranker owned by the Mixer that is used for all
  // other ranking.
  SearchResultRanker* GetNonAppSearchResultRanker();

  // Gets the length of the most recent query.
  int GetLastQueryLength() const;

  // Called when items in the results list have been on screen for some amount
  // of time, or the user clicked a search result.
  // TODO(959679): Rename this function to better reflect its nature.
  void OnSearchResultsDisplayed(
      const base::string16& trimmed_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int launched_index);

 private:
  // Invoked when the search results are changed.
  void OnResultsChanged();

  Profile* profile_;

  bool dispatching_query_ = false;

  // If true, the search results are shown on the launcher start page.
  bool query_for_recommendation_ = false;

  // The query associated with the most recent search.
  base::string16 last_query_;

  // The ID of the most recently launched app. This is used for app list launch
  // recording.
  std::string last_launched_app_id_;

  std::unique_ptr<Mixer> mixer_;
  using Providers = std::vector<std::unique_ptr<SearchProvider>>;
  Providers providers_;
  AppListControllerDelegate* list_controller_;

  DISALLOW_COPY_AND_ASSIGN(SearchController);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
