// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/macros.h"
#include "base/scoped_observer.h"

namespace app_list {

// Records impression, abandonment, and launch UMA metrics reported by the
// AppListNotifier.
class SearchMetricsObserver : ash::AppListNotifier::Observer {
 public:
  using Result = ash::AppListNotifier::Result;

  explicit SearchMetricsObserver(ash::AppListNotifier* notifier);
  ~SearchMetricsObserver() override;

  SearchMetricsObserver(const SearchMetricsObserver&) = delete;
  SearchMetricsObserver& operator=(const SearchMetricsObserver&) = delete;

  // AppListNotifier::Observer:
  void OnImpression(ash::AppListNotifier::Location location,
                    const std::vector<Result>& results,
                    const std::u16string& query) override;
  void OnAbandon(ash::AppListNotifier::Location location,
                 const std::vector<Result>& results,
                 const std::u16string& query) override;
  void OnLaunch(ash::AppListNotifier::Location location,
                const Result& launched,
                const std::vector<Result>& shown,
                const std::u16string& query) override;
  void OnIgnore(ash::AppListNotifier::Location location,
                const std::vector<Result>& results,
                const std::u16string& query) override;
  void OnQueryChanged(const std::u16string& query) override;

 private:
  ScopedObserver<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observer_{this};

  // Whether the search box currently contains an empty query.
  bool last_query_empty_ = true;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_
