// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_AGGREGATION_TAB_STRIP_SERVICE_TRACKER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_AGGREGATION_TAB_STRIP_SERVICE_TRACKER_H_

#include "base/functional/callback.h"
#include "base/functional/function_ref.h"

namespace tabs_api {

class TabStripService;

// An interface used by `TabStripServiceAggregator` to discover and
// track the lifecycle of `TabStripService` instances.
//
// Implementations of this class are responsible for monitoring and notifying
// the aggregator when services are created or destroyed.
class TabStripServiceTracker {
 public:
  using ServiceCallback = base::RepeatingCallback<void(TabStripService*)>;

  virtual ~TabStripServiceTracker() = default;

  virtual void SetOnAddedCallback(ServiceCallback on_added) = 0;
  virtual void SetOnRemovedCallback(ServiceCallback on_removed) = 0;

  // Returns a list of all services currently known to the tracker.
  virtual std::vector<TabStripService*> GetExistingServices() = 0;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_AGGREGATION_TAB_STRIP_SERVICE_TRACKER_H_
