// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_BROWSER_TAB_STRIP_SERVICE_TRACKER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_BROWSER_TAB_STRIP_SERVICE_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/aggregation/tab_strip_service_tracker.h"

class Profile;
class BrowserCollection;
class BrowserWindowInterface;

namespace tabs_api {

// A concrete implementation of `TabStripServiceTracker` that tracks
// `TabStripService` instances associated with browser windows.
//
// It observes a `BrowserCollection` for a specific `Profile` and extracts
// the `TabStripService` from each `BrowserWindowInterface`. This class acts
// as the bridge between the browser's window-tracking infrastructure and the
// TabStrip API aggregation system.
class BrowserTabStripServiceTracker : public TabStripServiceTracker,
                                      public BrowserCollectionObserver {
 public:
  using BrowserFilterCallback =
      base::RepeatingCallback<bool(BrowserWindowInterface*)>;

  BrowserTabStripServiceTracker(Profile* profile, BrowserFilterCallback filter);
  ~BrowserTabStripServiceTracker() override;

  BrowserTabStripServiceTracker(const BrowserTabStripServiceTracker&) = delete;
  BrowserTabStripServiceTracker& operator=(
      const BrowserTabStripServiceTracker&) = delete;

  // TabStripServiceTracker:
  void SetOnAddedCallback(ServiceCallback on_added) override;
  void SetOnRemovedCallback(ServiceCallback on_removed) override;
  std::vector<TabStripService*> GetExistingServices() override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  const raw_ptr<Profile> profile_;
  const BrowserFilterCallback filter_;
  ServiceCallback service_added_callback_;
  ServiceCallback service_removed_callback_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_BROWSER_TAB_STRIP_SERVICE_TRACKER_H_
