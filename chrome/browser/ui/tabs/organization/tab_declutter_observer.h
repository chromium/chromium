// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_OBSERVER_H_

#include <map>
#include <vector>

#include "base/observer_list_types.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;
}

class TabDeclutterObserver : public base::CheckedObserver {
 public:
  // Called when all checks pass to be able to show the tab declutter
  // nudge to action UI.
  virtual void OnTriggerDeclutterUIVisibility() {}

  // Called whenevener the service processes the tabstrip for unused tabs.
  virtual void OnUnusedTabsProcessed(
      std::vector<tabs::TabInterface*> stale_tabs,
      std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs) {}
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_OBSERVER_H_
