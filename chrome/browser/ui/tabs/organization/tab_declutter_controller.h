// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"

class TabStripModel;

namespace tabs {

// Controller that computes the tabs to be decluttered, tied to a specific
// browser.
class TabDeclutterController {
 public:
  explicit TabDeclutterController(TabStripModel* tab_strip_model);
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

  base::TimeDelta timer_interval_minutes() const {
    return timer_interval_minutes_;
  }

  void SetTimerForTesting(const base::TickClock* tick_clock,
                          scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void StartTimer();
  bool DeclutterNudgeCriteriaMet();
  void ProcessStaleTabs();

  base::TimeDelta stale_tab_threshold_duration_;
  base::TimeDelta timer_interval_minutes_;
  std::unique_ptr<base::RepeatingTimer> declutter_timer_;
  base::ObserverList<TabDeclutterObserver> observers_;
  raw_ptr<TabStripModel> tab_strip_model_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
