// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"

#include <memory>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace tabs {

namespace {
// Minimum number of duplicate tabs in the tabstrip that can be decluttered to
// show the nudge.
constexpr int kMinDeclutterableDuplicateTabCountForNudge = 3;
// Minimum number of inactive tabs in the tabstrip that can be decluttered to
// show the nudge.
constexpr int kMinTabCountForInactiveTabNudge = 15;
// Minimum percentage of stale tabs in the tabstrip to show the nudge.
constexpr double kStaleTabPercentageThreshold = 0.10;
}  // namespace

// static
void TabDeclutterController::EmitEntryPointHistogram(
    tab_search::mojom::TabDeclutterEntryPoint entry_point) {
  base::UmaHistogramEnumeration("Tab.Organization.Declutter.EntryPoint",
                                entry_point);
}

TabDeclutterController::TabDeclutterController(
    BrowserWindowInterface* browser_window_interface)
    : stale_tab_threshold_duration_(
          features::kTabstripDeclutterStaleThresholdDuration.Get()),
      declutter_timer_interval_(
          features::kTabstripDeclutterTimerInterval.Get()),
      nudge_timer_interval_(
          features::kTabstripDeclutterNudgeTimerInterval.Get()),
      declutter_timer_(std::make_unique<base::RepeatingTimer>()),
      usage_tick_clock_(std::make_unique<UsageTickClock>(
          base::DefaultTickClock::GetInstance())),
      next_nudge_valid_time_ticks_(usage_tick_clock_->NowTicks() +
                                   nudge_timer_interval_),
      tab_strip_model_(browser_window_interface->GetTabStripModel()),
      is_active_(false) {
  browser_subscriptions_.push_back(
      browser_window_interface->RegisterDidBecomeActive(base::BindRepeating(
          &TabDeclutterController::DidBecomeActive, base::Unretained(this))));
  browser_subscriptions_.push_back(
      browser_window_interface->RegisterDidBecomeInactive(base::BindRepeating(
          &TabDeclutterController::DidBecomeInactive, base::Unretained(this))));

  StartDeclutterTimer();
}

TabDeclutterController::~TabDeclutterController() = default;

void TabDeclutterController::StartDeclutterTimer() {
  declutter_timer_->Start(
      FROM_HERE, declutter_timer_interval_,
      base::BindRepeating(&TabDeclutterController::ProcessTabs,
                          base::Unretained(this)));
}

void TabDeclutterController::ProcessTabs() {
  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs;

  if (features::IsTabstripDedupeEnabled()) {
    duplicate_tabs = GetDuplicateTabs();
  }

  std::vector<tabs::TabInterface*> stale_tabs = GetStaleTabs();

  for (auto& observer : observers_) {
    observer.OnUnusedTabsProcessed(stale_tabs, duplicate_tabs);
  }

  if (DeclutterNudgeCriteriaMet(stale_tabs, duplicate_tabs)) {
    next_nudge_valid_time_ticks_ =
        usage_tick_clock_->NowTicks() + nudge_timer_interval_;

    for (auto& observer : observers_) {
      observer.OnTriggerDeclutterUIVisibility();
    }

    tabs_previous_nudge_.clear();
    tabs_previous_nudge_.insert(stale_tabs.begin(), stale_tabs.end());

    for (const auto& [url, tabs] : duplicate_tabs) {
      tabs_previous_nudge_.insert(tabs.begin(), tabs.end());
    }
  }
}

std::map<GURL, std::vector<tabs::TabInterface*>>
TabDeclutterController::GetDuplicateTabs() {
  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs;
  CHECK(features::IsTabstripDedupeEnabled());

  for (int tab_index = 0; tab_index < tab_strip_model_->GetTabCount();
       tab_index++) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(tab_index);

    if (IsTabExcluded(tab)) {
      continue;
    }

    if (tab->IsPinned() || tab->GetGroup().has_value()) {
      continue;
    }

    GURL url = tab->GetContents()->GetLastCommittedURL().GetWithoutRef();

    if (!url.is_valid()) {
      continue;
    }

    if (duplicate_tabs.find(url) != duplicate_tabs.end()) {
      duplicate_tabs[url].push_back(tab);
    } else {
      duplicate_tabs[url] = std::vector<tabs::TabInterface*>{tab};
    }
  }

  // Filter out entries with only 1 unique GURL.
  for (auto it = duplicate_tabs.begin(); it != duplicate_tabs.end();) {
    if (it->second.size() <= 1) {
      it = duplicate_tabs.erase(it);
    } else {
      ++it;
    }
  }
  return duplicate_tabs;
}

std::vector<tabs::TabInterface*> TabDeclutterController::GetStaleTabs() {
  CHECK(features::IsTabstripDeclutterEnabled());
  std::vector<tabs::TabInterface*> tabs;

  const base::Time now = base::Time::Now();
  for (int tab_index = 0; tab_index < tab_strip_model_->GetTabCount();
       tab_index++) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(tab_index);

    if (IsTabExcluded(tab)) {
      continue;
    }

    if (tab->IsPinned() || tab->GetGroup().has_value() ||
        tab->GetContents()->GetVisibility() == content::Visibility::VISIBLE) {
      continue;
    }

    auto* lifecycle_unit = resource_coordinator::TabLifecycleUnitSource::
        GetTabLifecycleUnitExternal(tab->GetContents());

    const base::Time last_focused_time = lifecycle_unit->GetLastFocusedTime();

    const base::TimeDelta elapsed = (last_focused_time == base::Time::Max())
                                        ? base::TimeDelta()
                                        : (now - last_focused_time);

    if (elapsed >= stale_tab_threshold_duration_) {
      tabs.push_back(tab);
    }
  }
  return tabs;
}

void TabDeclutterController::DeclutterTabs(
    std::vector<tabs::TabInterface*> tabs,
    const std::vector<GURL>& urls) {
  UMA_HISTOGRAM_COUNTS_1000("Tab.Organization.Declutter.DeclutterTabCount",
                            tabs.size());
  UMA_HISTOGRAM_COUNTS_1000("Tab.Organization.Declutter.TotalTabCount",
                            tab_strip_model_->count());
  UMA_HISTOGRAM_COUNTS_1000("Tab.Organization.Declutter.ExcludedTabCount",
                            excluded_tabs_.size());

  PrefService* prefs =
      Profile::FromBrowserContext(tab_strip_model_->profile())->GetPrefs();

  int usage_count =
      prefs->GetInteger(tab_search_prefs::kTabDeclutterUsageCount);
  prefs->SetInteger(tab_search_prefs::kTabDeclutterUsageCount, ++usage_count);

  base::UmaHistogramCounts1000("Tab.Organization.Declutter.TotalUsageCount",
                               usage_count);

  for (tabs::TabInterface* tab : tabs) {
    if (tab_strip_model_->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
      continue;
    }

    tab_strip_model_->CloseWebContentsAt(
        tab_strip_model_->GetIndexOfWebContents(tab->GetContents()),
        TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }

  for (GURL url : urls) {
    // TODO(crbug.com/376880738): Close all tabs with the given URL except for
    // the oldest.
  }

  excluded_tabs_.clear();
  excluded_urls_.clear();
}

void TabDeclutterController::DidBecomeActive(BrowserWindowInterface* browser) {
  is_active_ = true;
}

void TabDeclutterController::DidBecomeInactive(
    BrowserWindowInterface* browser) {
  is_active_ = false;
}

void TabDeclutterController::ExcludeFromStaleTabs(tabs::TabInterface* tab) {
  if (tab_strip_model_->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
    return;
  }

  excluded_tabs_.insert(tab);
}

void TabDeclutterController::ExcludeFromDuplicateTabs(GURL url) {
  if (!url.is_valid()) {
    return;
  }

  excluded_urls_.insert(url.GetWithoutRef());
}

bool TabDeclutterController::DeclutterNudgeCriteriaMet(
    base::span<tabs::TabInterface*> stale_tabs,
    std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs) {
  if (!is_active_ ||
      (usage_tick_clock_->NowTicks() < next_nudge_valid_time_ticks_)) {
    return false;
  }

  return HasNewUnusedTabsForNudge(stale_tabs, duplicate_tabs) &&
         (DeclutterStaleTabsNudgeCriteriaMet(stale_tabs) ||
          DeclutterDuplicateTabsNudgeCriteriaMet(duplicate_tabs));
}

bool TabDeclutterController::IsTabExcluded(tabs::TabInterface* tab) const {
  if (excluded_tabs_.find(tab) != excluded_tabs_.end()) {
    return true;
  }

  GURL url = tab->GetContents()->GetLastCommittedURL().GetWithoutRef();
  if (excluded_urls_.find(url) != excluded_urls_.end()) {
    return true;
  }

  return false;
}

bool TabDeclutterController::HasNewUnusedTabsForNudge(
    base::span<tabs::TabInterface*> stale_tabs,
    std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs) const {
  // Check for new unused stale tabs.
  if (IsNewTabDetectedForNudge(stale_tabs)) {
    return true;
  }

  // Check for new unused duplicate tabs.
  for (auto& [url, tabs] : duplicate_tabs) {
    if (IsNewTabDetectedForNudge(base::span(tabs))) {
      return true;
    }
  }

  return false;
}

bool TabDeclutterController::IsNewTabDetectedForNudge(
    base::span<tabs::TabInterface*> tabs) const {
  for (tabs::TabInterface* tab : tabs) {
    if (tabs_previous_nudge_.count(tab) == 0) {
      return true;
    }
  }
  return false;
}

bool TabDeclutterController::DeclutterStaleTabsNudgeCriteriaMet(
    base::span<tabs::TabInterface*> stale_tabs) {
  if (stale_tabs.empty()) {
    return false;
  }

  const int total_tab_count = tab_strip_model_->GetTabCount();

  if (total_tab_count < kMinTabCountForInactiveTabNudge) {
    return false;
  }

  const int required_stale_tabs = static_cast<int>(
      std::ceil(total_tab_count * kStaleTabPercentageThreshold));

  if (static_cast<int>(stale_tabs.size()) < required_stale_tabs) {
    return false;
  }

  return true;
}

bool TabDeclutterController::DeclutterDuplicateTabsNudgeCriteriaMet(
    std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs) {
  int declutterable_duplicate_tabs = 0;
  for (const auto& [url, tabs] : duplicate_tabs) {
    // The original duplicate tab is not declutterable.
    declutterable_duplicate_tabs += tabs.size() - 1;
  }

  return declutterable_duplicate_tabs >=
         kMinDeclutterableDuplicateTabCountForNudge;
}

void TabDeclutterController::OnActionUIDismissed(
    base::PassKey<TabSearchContainer>) {
  nudge_timer_interval_ = nudge_timer_interval_ * 2;
  next_nudge_valid_time_ticks_ =
      usage_tick_clock_->NowTicks() + nudge_timer_interval_;
}

void TabDeclutterController::SetTimerForTesting(
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  declutter_timer_->Stop();
  declutter_timer_ = std::make_unique<base::RepeatingTimer>(tick_clock);
  declutter_timer_->SetTaskRunner(task_runner);
  StartDeclutterTimer();

  usage_tick_clock_.reset();
  usage_tick_clock_ = std::make_unique<UsageTickClock>(tick_clock);
  next_nudge_valid_time_ticks_ =
      usage_tick_clock_->NowTicks() + nudge_timer_interval_;
}
}  // namespace tabs
