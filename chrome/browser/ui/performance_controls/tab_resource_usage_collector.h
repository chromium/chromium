// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_COLLECTOR_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_COLLECTOR_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/performance_manager/public/resource_attribution/queries.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class WebContents;
}

class TabResourceUsageCollector
    : public resource_attribution::QueryResultObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Raised after the tab resource metrics have refreshed through an immediate
    // or periodic query made by the TabResourceUsageCollector
    virtual void OnTabResourceMetricsRefreshed() = 0;
  };

  ~TabResourceUsageCollector() override;

  static TabResourceUsageCollector* Get();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  void ImmediatelyRefreshMetrics(content::WebContents* web_contents);
  void ImmediatelyRefreshMetricsForAllTabs();

  // resource_attribution::QueryResultObserver:
  void OnResourceUsageUpdated(
      const resource_attribution::QueryResultMap& results) override;

 private:
  friend base::NoDestructor<TabResourceUsageCollector>;

  TabResourceUsageCollector();

  resource_attribution::ScopedResourceUsageQuery scoped_query_;
  resource_attribution::ScopedQueryObservation query_observer_{this};
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_COLLECTOR_H_
