// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_LIST_INTERFACE_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_LIST_INTERFACE_OBSERVER_H_

#include "base/observer_list_types.h"

namespace tabs {
class TabInterface;
}

class TabListInterfaceObserver : public base::CheckedObserver {
 public:
  // Called when a new tab is added to the tab list. `tab` is the newly-added
  // tab, and `index` is the index at which it was added. Note that this doesn't
  // necessarily mean `tab` is a newly-created tab; it may have moved from a
  // different tab list.
  // TODO(https://crbug.com/433545116): This may not be called in all situations
  // on Android platforms, such as if a tab that was closed is re-introduced
  // (see also tabClosureUndone() here:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/tabmodel/android/java/src/org/chromium/chrome/browser/tabmodel/TabModelObserver.java;drc=e2bb611334ebe2b1cbe519ff183f5178896b8c55;l=140).
  virtual void OnTabAdded(tabs::TabInterface* tab, int index) {}

  // Called when the active tab changed. `tab` is the new active tab and is
  // never null.
  virtual void OnActiveTabChanged(tabs::TabInterface* tab) {}
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_LIST_INTERFACE_OBSERVER_H_
