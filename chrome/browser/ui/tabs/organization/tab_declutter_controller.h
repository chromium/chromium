// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"

class TabStripModel;
class TabSearchContainer;

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

  base::TimeDelta declutter_timer_interval_minutes() const {
    return declutter_timer_interval_minutes_;
  }

  base::TimeTicks next_nudge_valid_time_ticks() const {
    return next_nudge_valid_time_ticks_;
  }

  base::TimeDelta nudge_timer_interval_minutes() const {
    return nudge_timer_interval_minutes_;
  }

  void OnActionUIDismissed(base::PassKey<TabSearchContainer>);

  void SetTimerForTesting(const base::TickClock* tick_clock,
                          scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void StartDeclutterTimer();
  bool DeclutterNudgeCriteriaMet(const std::vector<tabs::TabModel*> stale_tabs);
  void ProcessStaleTabs();
  void StartNudgeTimer();

  // Duration of inactivity after which a tab is considered stale.
  base::TimeDelta stale_tab_threshold_duration_;
  // Interval between a recomputation of stale tabs.
  base::TimeDelta declutter_timer_interval_minutes_;
  // Interval after showing a nudge to prevent another nudge from being shown.
  base::TimeDelta nudge_timer_interval_minutes_;
  // The timer that is responsible for calculating stale tabs on getting
  // triggered.
  std::unique_ptr<base::RepeatingTimer> declutter_timer_;
  // The usage tick clock that is used for setting
  // `next_nudge_valid_time_ticks_` and comparing time ticks with
  // `next_nudge_valid_time_ticks_` to show the nudge.
  std::unique_ptr<UsageTickClock> usage_tick_clock_;
  // The timer that is responsible for blocking the nudge from showing.
  base::TimeTicks next_nudge_valid_time_ticks_;
  // The list of tabs shown previously in a nudge.
  std::set<tabs::TabModel*> stale_tabs_previous_nudge_;

  base::ObserverList<TabDeclutterObserver> observers_;
  raw_ptr<TabStripModel> tab_strip_model_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
