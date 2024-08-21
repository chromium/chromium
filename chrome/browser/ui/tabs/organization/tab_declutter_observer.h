// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_OBSERVER_H_

#include <vector>

#include "base/observer_list_types.h"

namespace tabs {
class TabModel;
}

class Browser;

class TabDeclutterObserver : public base::CheckedObserver {
 public:
  // Called when all checks pass to be able to show or hide the tab declutter
  // nudge to action UI.
  virtual void OnTriggerDeclutterUIVisibility(const Browser* browser,
                                              bool should_show) {}

  // Called whenevener the service processes the tabstrip for out of date tabs.
  virtual void OnOutOfDateTabsProcessed(const Browser* browser,
                                        std::vector<tabs::TabModel*> tabs) {}
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_OBSERVER_H_
