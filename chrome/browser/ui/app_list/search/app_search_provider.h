// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class Profile;

namespace base {
class Clock;
}

namespace app_list {

class AppSearchDataSource;

class AppSearchProvider : public SearchProvider {
 public:
  // |clock| should be used by tests that needs to overrides the time.
  // Otherwise, pass a base::DefaultClock instance. This doesn't take the
  // ownership of the clock. |clock| must outlive the AppSearchProvider
  // instance.
  AppSearchProvider(Profile* profile,
                    AppListControllerDelegate* list_controller,
                    base::Clock* clock,
                    AppListModelUpdater* model_updater);

  AppSearchProvider(const AppSearchProvider&) = delete;
  AppSearchProvider& operator=(const AppSearchProvider&) = delete;

  ~AppSearchProvider() override;

  // SearchProvider overrides:
  void Start(const std::u16string& query) override;
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;
  bool ShouldBlockZeroState() const override;

 private:
  void UpdateResults();

  // Updates the zero-state app recommendations ("recent apps").
  void UpdateRecommendedResults(
      const base::flat_map<std::string, uint16_t>& id_to_app_list_index);

  void UpdateQueriedResults();

  // Publishes either the queried results or recommendation.
  // |is_queried_search|: true for queried results, false for recommendation.
  void PublishQueriedResultsOrRecommendation(bool is_queried_search,
                                             Results* new_results);

  // Records the app search provider's latency when user initiates a search or
  // gets the zero state suggestions.
  // If |is_queried_search| is true, record query latency; otherwise, record
  // zero state recommendation latency.
  void MaybeRecordQueryLatencyHistogram(bool is_queried_search);

  std::u16string query_;
  base::TimeTicks query_start_time_;
  bool record_query_uma_ = false;
  AppListModelUpdater* const model_updater_;
  std::unique_ptr<AppSearchDataSource> data_source_;

  // Used to skip result updates caused by data source changes due to an
  // explicit refresh request.
  bool updates_blocked_ = false;

  base::CallbackListSubscription app_updates_subscription_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
