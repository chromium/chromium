// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_AGGREGATION_TAB_STRIP_SERVICE_AGGREGATOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_AGGREGATION_TAB_STRIP_SERVICE_AGGREGATOR_H_

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/aggregation/tab_strip_service_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"

namespace tabs_api {

// Manages observations. It uses `TabStripServiceTracker` to find the services
// and then subscribes and unifies the event streams from all of them.
class TabStripServiceAggregator
    : public observation::TabStripApiBatchedObserver {
 public:
  using OnTabEventsCallback =
      base::RepeatingCallback<void(const std::vector<mojom::TabsEventPtr>&)>;

  TabStripServiceAggregator(std::unique_ptr<TabStripServiceTracker> tracker,
                            OnTabEventsCallback on_events_callback);
  ~TabStripServiceAggregator() override;

  TabStripServiceAggregator(const TabStripServiceAggregator&) = delete;
  TabStripServiceAggregator& operator=(const TabStripServiceAggregator&) =
      delete;

  // TabStripApiBatchedObserver:
  void OnTabEvents(const std::vector<mojom::TabsEventPtr>& events) override;

 private:
  void OnServiceAdded(TabStripService* service);
  void OnServiceRemoved(TabStripService* service);

  std::unique_ptr<TabStripServiceTracker> tracker_;
  OnTabEventsCallback on_events_callback_;

  // Tracks which services are currently being observed.
  std::set<raw_ptr<TabStripService>> observed_services_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_AGGREGATION_TAB_STRIP_SERVICE_AGGREGATOR_H_
