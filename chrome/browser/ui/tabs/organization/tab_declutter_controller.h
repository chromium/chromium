// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"

namespace content {
class BrowserContext;
}

class TabStripModel;

namespace tabs {

// Controller that computes the tabs to be decluttered, tied to a specific
// browser.
class TabDeclutterController {
 public:
  TabDeclutterController(TabStripModel* tab_strip_model,
                         content::BrowserContext* browser_context);
  TabDeclutterController(const TabDeclutterController&) = delete;
  TabDeclutterController& operator=(const TabDeclutterController& other) =
      delete;
  ~TabDeclutterController();

  void AddObserver(TabDeclutterObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(TabDeclutterObserver* observer) {
    observers_.RemoveObserver(observer);
  }
  bool HasObserver(TabDeclutterObserver* observer) {
    return observers_.HasObserver(observer);
  }

  base::TimeDelta stale_tab_threshold_duration() const {
    return stale_tab_threshold_duration_;
  }

  // TODO(b/362310942): Make this method private after adding a timer.
  void ProcessStaleTabs();

 private:
  bool DeclutterNudgeCriteriaMet();

  base::TimeDelta stale_tab_threshold_duration_;
  base::ObserverList<TabDeclutterObserver> observers_;
  raw_ptr<TabStripModel> tab_strip_model_;
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
