// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_SERVICE_TRACKER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_SERVICE_TRACKER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/aggregation/tab_strip_service_tracker.h"

namespace tabs_api::testing {

class ToyTabStripServiceTracker : public TabStripServiceTracker {
 public:
  ToyTabStripServiceTracker();
  ~ToyTabStripServiceTracker() override;

  // TabStripServiceTracker:
  void SetOnAddedCallback(ServiceCallback on_added) override;
  void SetOnRemovedCallback(ServiceCallback on_removed) override;
  std::vector<TabStripService*> GetExistingServices() override;

  // Helper for tests to add services and notify observers.
  void AddService(TabStripService* service);

  // Helper for tests to remove services and notify observers.
  void RemoveService(TabStripService* service);

 private:
  std::vector<raw_ptr<TabStripService>> services_;
  ServiceCallback on_service_added_callback_;
  ServiceCallback on_service_removed_callback_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_SERVICE_TRACKER_H_
