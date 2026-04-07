// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_TAB_LIST_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_TAB_LIST_OBSERVER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"

class SidePanelCoordinatorAndroid;

// Observes changes in the tab list (e.g., active tab changes) and notifies
// `SidePanelCoordinatorAndroid` to update the side panel UI accordingly.
//
// Note:
// * This class has a circular dependency with `SidePanelCoordinatorAndroid` as
//   they are co-dependent and designed to work together.
// * This class is tested alongside `SidePanelCoordinatorAndroid` in
//   `side_panel_coordinator_android_browsertest.cc`.
class SidePanelTabListObserverAndroid final : public TabListInterfaceObserver {
 public:
  SidePanelTabListObserverAndroid(TabListInterface* tab_list,
                                  SidePanelCoordinatorAndroid* coordinator);
  ~SidePanelTabListObserverAndroid() override;

  SidePanelTabListObserverAndroid(const SidePanelTabListObserverAndroid&) =
      delete;
  SidePanelTabListObserverAndroid& operator=(
      const SidePanelTabListObserverAndroid&) = delete;

 private:
  // Implements `TabListInterfaceObserver`:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  const raw_ptr<SidePanelCoordinatorAndroid> coordinator_;

  // `TabHandle` for the current active tab.
  //
  // We need to cache the active tab handle because
  // `TabListInterfaceObserver:: OnActiveTabChanged()` does not provide the
  // previous active tab.
  //
  // Note:
  //
  // We shouldn't cache a `TabInterface*` as closing a tab can also trigger an
  // active tab change, and the cached `TabInterface*` will be invalid in that
  // case.
  //
  // We also shouldn't cache the "active tab index" as it may not refer to the
  // right tab after an active tab change.
  // For example, if we have tabs [tab_0, tab_1, tab_2], and tab_1 is the
  // active tab.
  // If we cache the active tab index, its initial value would be 1.
  // If tab_1 is closed, we will have [tab_0, tab_2], and the active tab will
  // become tab_2, but the new active tab index is still 1 (same as the
  // initial active tab index). In this case, it's hard to tell if the active
  // tab has changed or not.
  tabs::TabHandle active_tab_handle_;

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_TAB_LIST_OBSERVER_ANDROID_H_
