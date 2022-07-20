// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace ash {
enum class AppListSearchResultType;
}  // namespace ash

namespace app_list {

class SearchMetricsObserver;
class SearchProvider;
enum class RankingItemType;

// TODO(crbug.com/1199206): This is the old implementation of the search
// controller. Once we have fully migrated to the new system, this can be
// cleaned up.
class SearchControllerImpl : public SearchController,
                             public ash::AppListNotifier::Observer {
 public:
  using ResultsChangedCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType)>;

  SearchControllerImpl(AppListModelUpdater* model_updater,
                       AppListControllerDelegate* list_controller,
                       ash::AppListNotifier* notifier,
                       Profile* profile);
  ~SearchControllerImpl() override;

  SearchControllerImpl(const SearchControllerImpl&) = delete;
  SearchControllerImpl& operator=(const SearchControllerImpl&) = delete;

  // SearchController:
  void InitializeRankers() override;
  void StartSearch(const std::u16string& query) override;
  void StartZeroState(base::OnceClosure on_done,
                      base::TimeDelta timeout) override;
  void ViewClosing() override;
  void OpenResult(ChromeSearchResult* result, int event_flags) override;
  void InvokeResultAction(ChromeSearchResult* result,
                          ash::SearchResultActionType action) override;
  size_t AddGroup(size_t max_results) override;
  void AddProvider(size_t group_id,
                   std::unique_ptr<SearchProvider> provider) override;
  void SetResults(const SearchProvider* provider, Results results) override;
  void Publish() override;
  ChromeSearchResult* FindSearchResult(const std::string& result_id) override;
  ChromeSearchResult* GetResultByTitleForTest(
      const std::string& title) override;
  void Train(LaunchData&& launch_data) override;
  int GetLastQueryLength() const override;
  void AddObserver(SearchController::Observer* observer) override;
  void RemoveObserver(SearchController::Observer* observer) override;
  std::u16string get_query() override;
  base::Time session_start() override;
  void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) override;
  void disable_ranking_for_test() override;

  // ash::AppListNotifier::Observer:
  void OnImpression(ash::AppListNotifier::Location location,
                    const std::vector<ash::AppListNotifier::Result>& results,
                    const std::u16string& query) override;

  void NotifyResultsAdded(const std::vector<ChromeSearchResult*>& results);

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

  // The time when StartSearch was most recently called.
  base::Time session_start_;

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
  ash::AppListNotifier* const notifier_;
  base::ObserverList<SearchController::Observer> observer_list_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_H_
