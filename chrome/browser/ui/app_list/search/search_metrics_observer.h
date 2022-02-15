// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/scoped_observation.h"

class ChromeSearchResult;
class Profile;

namespace app_list {

// Records launcher search backend metrics. This includes impression,
// abandonment, and launch information reported by the AppListNotifier.
//
// TODO(crbug.com/1258415): Rename this SearchMetricsManager.
class SearchMetricsObserver : ash::AppListNotifier::Observer {
 public:
  using Result = ash::AppListNotifier::Result;
  using Location = ash::AppListNotifier::Location;

  SearchMetricsObserver(Profile* profile, ash::AppListNotifier* notifier);
  ~SearchMetricsObserver() override;

  SearchMetricsObserver(const SearchMetricsObserver&) = delete;
  SearchMetricsObserver& operator=(const SearchMetricsObserver&) = delete;

  // ash::AppListNotifier::Observer:
  void OnImpression(Location location,
                    const std::vector<Result>& results,
                    const std::u16string& query) override;
  void OnAbandon(Location location,
                 const std::vector<Result>& results,
                 const std::u16string& query) override;
  void OnLaunch(Location location,
                const Result& launched,
                const std::vector<Result>& shown,
                const std::u16string& query) override;
  void OnIgnore(Location location,
                const std::vector<Result>& results,
                const std::u16string& query) override;

 private:
  base::ScopedObservation<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observation_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_
