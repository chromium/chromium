// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_

#include <memory>
#include <optional>
#include <set>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom-forward.h"
#include "url/gurl.h"

class TabStripActionContainer;
class TabStripModel;
class TabSearchContainer;
class BrowserWindowInterface;

namespace tabs {

class TabInterface;

// Controller that computes the tabs to be decluttered, tied to a specific
// browser.
class TabDeclutterController {
 public:
  static void EmitEntryPointHistogram(
      tab_search::mojom::TabDeclutterEntryPoint entry_point);

  explicit TabDeclutterController(
      BrowserWindowInterface* browser_window_interface);
  TabDeclutterController(const TabDeclutterController&) = delete;
  TabDeclutterController& operator=(const TabDeclutterController& other) =
      delete;
  virtual ~TabDeclutterController();

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

  base::RepeatingTimer* GetDeclutterTimerForTesting() const {
    return declutter_timer_.get();
  }

  base::TimeDelta declutter_timer_interval() const {
    return declutter_timer_interval_;
  }

  base::TimeTicks next_nudge_valid_time_ticks() const {
    return next_nudge_valid_time_ticks_;
  }

  void set_next_nudge_valid_time_ticks_for_testing(
      base::TimeTicks next_nudge_valid_time_ticks) {
    next_nudge_valid_time_ticks_ = next_nudge_valid_time_ticks;
  }

  base::TimeDelta nudge_timer_interval() const { return nudge_timer_interval_; }

  void OnActionUIDismissed(base::PassKey<TabSearchContainer>);
  void OnActionUIDismissed(base::PassKey<TabStripActionContainer>);

  void SetTimerForTesting(const base::TickClock* tick_clock,
                          scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual std::vector<tabs::TabInterface*> GetStaleTabs();

  virtual std::map<GURL, std::vector<tabs::TabInterface*>> GetDuplicateTabs();

  TabStripModel* tab_strip_model() { return tab_strip_model_; }

  void ExcludeFromStaleTabs(tabs::TabInterface* tabs);

  void ExcludeFromDuplicateTabs(GURL url);

  void DidBecomeActive(BrowserWindowInterface* browser_window_interface);

  void DidBecomeInactive(BrowserWindowInterface* browser_window_interface);

  // Closes the tabs from the tabstrip if they are present.
  void DeclutterTabs(std::vector<tabs::TabInterface*> tabs,
                     const std::vector<GURL>& urls);

 private:
  void StartDeclutterTimer();

  // Returns whether the nudge should be shown in the tabstrip for declutter.
  // Note: The calculation for stale tabs and duplicate tabs are considered
  // independent of each other from the perspective of the controller. This
  // means that the expected unused tabs for nudge might be different from that
  // of the webUI in certain edge cases where a duplicate tab is also a stale
  // tab.
  bool DeclutterNudgeCriteriaMet(
      base::span<tabs::TabInterface*> stale_tabs,
      std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs);

  bool DeclutterStaleTabsNudgeCriteriaMet(
      base::span<tabs::TabInterface*> stale_tabs);

  bool DeclutterDuplicateTabsNudgeCriteriaMet(
      std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs);

  // Helper for computing `DeclutterNudgeCriteriaMet()` by calculating if there
  // is a new unused tab from the previous nudge.
  bool HasNewUnusedTabsForNudge(
      base::span<tabs::TabInterface*> stale_tabs,
      std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs) const;

  // Returns if `tabs` has an element not present in `tabs_previous_nudge_`.
  bool IsNewTabDetectedForNudge(base::span<tabs::TabInterface*> tabs) const;
  void ProcessTabs();

  void StartNudgeTimer();

  void LogExcludedDuplicateTabMetrics();

  bool IsTabExcluded(tabs::TabInterface* tab) const;

  void ResetAndDoubleNudgeTimer();

  // Duration of inactivity after which a tab is considered stale.
  base::TimeDelta stale_tab_threshold_duration_;
  // Interval between a recomputation of stale tabs.
  base::TimeDelta declutter_timer_interval_;
  // Interval after showing a nudge to prevent another nudge from being shown.
  base::TimeDelta nudge_timer_interval_;
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
  std::set<tabs::TabInterface*> tabs_previous_nudge_;

  base::ObserverList<TabDeclutterObserver> observers_;
  raw_ptr<TabStripModel> tab_strip_model_;
  std::set<tabs::TabInterface*> excluded_tabs_;
  std::set<GURL> excluded_urls_;

  bool is_active_;
  // Holds subscriptions for BrowserWindowInterface callbacks.
  std::vector<base::CallbackListSubscription> browser_subscriptions_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_CONTROLLER_H_
