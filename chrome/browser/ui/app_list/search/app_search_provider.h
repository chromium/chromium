// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class Profile;

namespace base {
class Clock;
}

namespace sync_sessions {
class OpenTabsUIDelegate;
}  // namespace sync_sessions

namespace app_list {

class AppSearchProvider : public SearchProvider {
 public:
  class App;
  class DataSource;
  using Apps = std::vector<std::unique_ptr<App>>;

  // |clock| should be used by tests that needs to overrides the time.
  // Otherwise, pass a base::DefaultClock instance. This doesn't take the
  // ownership of the clock. |clock| must outlive the AppSearchProvider
  // instance.
  AppSearchProvider(Profile* profile,
                    AppListControllerDelegate* list_controller,
                    base::Clock* clock,
                    AppListModelUpdater* model_updater);
  ~AppSearchProvider() override;

  // SearchProvider overrides:
  void Start(const base::string16& query) override;
  void ViewClosing() override;

  // Refreshes apps and updates results inline
  void RefreshAppsAndUpdateResults();

  // Refreshes apps deferred to prevent multiple redundant refreshes in case of
  // batch update events from app providers. Used in case when no removed app is
  // detected.
  void RefreshAppsAndUpdateResultsDeferred();

  void set_open_tabs_ui_delegate_for_testing(
      sync_sessions::OpenTabsUIDelegate* delegate) {
    open_tabs_ui_delegate_for_testing_ = delegate;
  }
  sync_sessions::OpenTabsUIDelegate* open_tabs_ui_delegate_for_testing() {
    return open_tabs_ui_delegate_for_testing_;
  }

 private:
  void UpdateResults();
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

  Profile* profile_;
  AppListControllerDelegate* const list_controller_;
  base::string16 query_;
  base::TimeTicks query_start_time_;
  bool record_query_uma_ = false;
  Apps apps_;
  AppListModelUpdater* const model_updater_;
  base::Clock* clock_;
  std::vector<std::unique_ptr<DataSource>> data_sources_;
  sync_sessions::OpenTabsUIDelegate* open_tabs_ui_delegate_for_testing_ =
      nullptr;
  base::WeakPtrFactory<AppSearchProvider> refresh_apps_factory_{this};
  base::WeakPtrFactory<AppSearchProvider> update_results_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
