// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_NEW_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_NEW_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace ash {
class AppListNotifier;
enum class AppListSearchResultType;
}  // namespace ash

namespace app_list {

class SearchMetricsObserver;
class SearchProvider;
enum class RankingItemType;

// TODO(crbug.com/1199206): This is the new implementation of the search
// controller. Once we have fully migrated to the new system, this can replace
// SearchController.
class SearchControllerImplNew : public SearchController {
 public:
  using ResultsChangedCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType)>;

  SearchControllerImplNew(AppListModelUpdater* model_updater,
                          AppListControllerDelegate* list_controller,
                          ash::AppListNotifier* notifier,
                          Profile* profile);
  ~SearchControllerImplNew() override;

  SearchControllerImplNew(const SearchControllerImplNew&) = delete;
  SearchControllerImplNew& operator=(const SearchControllerImplNew&) = delete;

  // SearchController:
  void StartSearch(const std::u16string& query) override;
  void StartZeroState(base::OnceClosure on_done,
                      base::TimeDelta timeout) override;
  void OpenResult(ChromeSearchResult* result, int event_flags) override;
  void InvokeResultAction(ChromeSearchResult* result,
                          ash::SearchResultActionType action) override;
  size_t AddGroup(size_t max_results) override;
  void AddProvider(size_t group_id,
                   std::unique_ptr<SearchProvider> provider) override;
  void SetResults(ash::AppListSearchResultType provider_type,
                  Results results) override;
  ChromeSearchResult* FindSearchResult(const std::string& result_id) override;
  ChromeSearchResult* GetResultByTitleForTest(
      const std::string& title) override;
  void Train(LaunchData&& launch_data) override;
  void AppListShown() override;
  void ViewClosing() override;
  int GetLastQueryLength() const override;
  void OnSearchResultsImpressionMade(
      const std::u16string& trimmed_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int launched_index) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) override;
  std::u16string get_query() override;
  base::Time session_start() override;

  void set_ranker_delegate_for_test(
      std::unique_ptr<RankerDelegate> ranker_delegate) {
    ranker_ = std::move(ranker_delegate);
  }

 private:
  void RankAndPublish(const ash::AppListSearchResultType provider_type);

  Profile* profile_;

  // The query associated with the most recent search.
  std::u16string last_query_;

  // The time when Start was most recently called.
  base::Time session_start_;

  // The ID of the most recently launched app. This is used for app list launch
  // recording.
  std::string last_launched_app_id_;

  // Top-level result ranker.
  std::unique_ptr<RankerDelegate> ranker_;

  // Storage for all search results for the current query.
  ResultsMap results_;

  // Storage for category scores for the current query.
  CategoriesList categories_;

  std::unique_ptr<SearchMetricsObserver> metrics_observer_;
  using Providers = std::vector<std::unique_ptr<SearchProvider>>;
  Providers providers_;
  AppListModelUpdater* const model_updater_;
  AppListControllerDelegate* const list_controller_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_NEW_H_
