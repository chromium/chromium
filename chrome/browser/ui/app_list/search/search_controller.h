// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace ash {
class AppListNotifier;
enum class AppListSearchResultType;
}

namespace app_list {

class SearchMetricsObserver;
class SearchProvider;
enum class RankingItemType;

// Controller that collects query from given SearchBoxModel, dispatches it
// to all search providers, then invokes the mixer to mix and to publish the
// results to the given SearchResults UI model.
class SearchController {
 public:
  using ResultsChangedCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType)>;

  SearchController(AppListModelUpdater* model_updater,
                   AppListControllerDelegate* list_controller,
                   ash::AppListNotifier* notifier,
                   Profile* profile);
  virtual ~SearchController();

  void InitializeRankers();

  void Start(const std::u16string& query);
  void ViewClosing();

  void OpenResult(ChromeSearchResult* result, int event_flags);
  void InvokeResultAction(ChromeSearchResult* result, int action_index);

  // Adds a new mixer group. See Mixer::AddGroup.
  size_t AddGroup(size_t max_results);

  // Takes ownership of |provider| and associates it with given mixer group.
  void AddProvider(size_t group_id, std::unique_ptr<SearchProvider> provider);

  virtual ChromeSearchResult* FindSearchResult(const std::string& result_id);
  ChromeSearchResult* GetResultByTitleForTest(const std::string& title);

  // Sends training signal to each |providers_|
  void Train(AppLaunchData&& app_launch_data);

  // Invoked when the app list is shown.
  void AppListShown();

  // Gets the length of the most recent query.
  int GetLastQueryLength() const;

  // Called when items in the results list have been on screen for some amount
  // of time, or the user clicked a search result.
  void OnSearchResultsImpressionMade(
      const std::u16string& trimmed_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int launched_index);

  void set_results_changed_callback_for_test(ResultsChangedCallback callback) {
    results_changed_callback_ = std::move(callback);
  }

 private:
  // Invoked when the search results are changed. Providers should use the one
  // argument version, and pass the primary type of result produced by the
  // invoking search provider.
  void OnResultsChanged();
  void OnResultsChangedWithType(ash::AppListSearchResultType result_type);

  Profile* profile_;

  bool dispatching_query_ = false;

  // If true, the search results are shown on the launcher start page.
  bool query_for_recommendation_ = false;

  // The query associated with the most recent search.
  std::u16string last_query_;

  // The ID of the most recently launched app. This is used for app list launch
  // recording.
  std::string last_launched_app_id_;

  // If set, called when OnResultsChanged is invoked.
  ResultsChangedCallback results_changed_callback_;

  std::unique_ptr<Mixer> mixer_;
  std::unique_ptr<SearchMetricsObserver> metrics_observer_;
  using Providers = std::vector<std::unique_ptr<SearchProvider>>;
  Providers providers_;
  AppListControllerDelegate* list_controller_;

  DISALLOW_COPY_AND_ASSIGN(SearchController);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
