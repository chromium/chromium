// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"

#include <memory>

#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "content/public/browser/browser_context.h"

namespace tabs {

namespace {
// TODO(b/362269642): Make this constant finch configurable.

// Duration of inactivity after which a tab is considered stale.
constexpr int kStaleThresholdDurationDays = 7;
// Interval between a recomputation of stale tabs.
constexpr base::TimeDelta kTimerIntervalMinutes = base::Minutes(10);
// Minimum number of tabs in the tabstrip to show the nudge.
constexpr int kMinTabCountForNudge = 15;
// Minimum percentage of stale tabs in the tabstrip to show the nudge.
constexpr double kStaleTabPercentageThreshold = 0.10;
// Default interval after showing a nudge to prevent another nudge from being
// shown.
constexpr base::TimeDelta kDefaultNudgeTimerIntervalMinutes =
    base::Minutes(6 * 60);
}  // namespace

TabDeclutterController::TabDeclutterController(TabStripModel* tab_strip_model)
    : stale_tab_threshold_duration_(base::Days(kStaleThresholdDurationDays)),
      declutter_timer_interval_minutes_(kTimerIntervalMinutes),
      nudge_timer_interval_minutes_(kDefaultNudgeTimerIntervalMinutes),
      declutter_timer_(std::make_unique<base::RepeatingTimer>()),
      usage_tick_clock_(std::make_unique<UsageTickClock>(
          base::DefaultTickClock::GetInstance())),
      next_nudge_valid_time_ticks_(usage_tick_clock_->NowTicks() +
                                   nudge_timer_interval_minutes_),
      tab_strip_model_(tab_strip_model) {
  StartDeclutterTimer();
}

TabDeclutterController::~TabDeclutterController() {}

void TabDeclutterController::StartDeclutterTimer() {
  declutter_timer_->Start(
      FROM_HERE, declutter_timer_interval_minutes_,
      base::BindRepeating(&TabDeclutterController::ProcessStaleTabs,
                          base::Unretained(this)));
}

void TabDeclutterController::ProcessStaleTabs() {
  CHECK(features::IsTabstripDeclutterEnabled());
  std::vector<tabs::TabModel*> tabs;

  const base::Time now = base::Time::Now();
  for (int tab_index = 0; tab_index < tab_strip_model_->GetTabCount();
       tab_index++) {
    tabs::TabModel* tab_model = tab_strip_model_->GetTabAtIndex(tab_index);

    if (tab_model->pinned() || tab_model->group().has_value()) {
      continue;
    }

    auto* lifecycle_unit = resource_coordinator::TabLifecycleUnitSource::
        GetTabLifecycleUnitExternal(tab_model->contents());

    const base::Time last_focused_time = lifecycle_unit->GetLastFocusedTime();

    const base::TimeDelta elapsed = (last_focused_time == base::Time::Max())
                                        ? base::TimeDelta()
                                        : (now - last_focused_time);

    if (elapsed >= stale_tab_threshold_duration_) {
      tabs.push_back(tab_model);
    }
  }

  for (auto& observer : observers_) {
    observer.OnStaleTabsProcessed(tabs);
  }

  if (DeclutterNudgeCriteriaMet(tabs)) {
    next_nudge_valid_time_ticks_ =
        usage_tick_clock_->NowTicks() + nudge_timer_interval_minutes_;
    for (auto& observer : observers_) {
      observer.OnTriggerDeclutterUIVisibility(!tabs.empty());
    }

    stale_tabs_previous_nudge_.clear();
    stale_tabs_previous_nudge_.insert(tabs.begin(), tabs.end());
  }
}

bool TabDeclutterController::DeclutterNudgeCriteriaMet(
    const std::vector<tabs::TabModel*> stale_tabs) {
  if (usage_tick_clock_->NowTicks() < next_nudge_valid_time_ticks_) {
    return false;
  }

  // TODO(b/366078827): Handle hide case for the nudge.
  if (stale_tabs.empty()) {
    return false;
  }

  const int total_tab_count = tab_strip_model_->GetTabCount();

  if (total_tab_count < kMinTabCountForNudge) {
    return false;
  }

  const int required_stale_tabs = static_cast<int>(
      std::ceil(total_tab_count * kStaleTabPercentageThreshold));

  if (static_cast<int>(stale_tabs.size()) < required_stale_tabs) {
    return false;
  }

  // If there is a new stale tab found in this computation, return true.
  for (tabs::TabModel* tab : stale_tabs) {
    if (stale_tabs_previous_nudge_.count(tab) == 0) {
      return true;
    }
  }

  return false;
}

void TabDeclutterController::OnActionUIDismissed(
    base::PassKey<TabSearchContainer>) {
  nudge_timer_interval_minutes_ = nudge_timer_interval_minutes_ * 2;
  next_nudge_valid_time_ticks_ =
      usage_tick_clock_->NowTicks() + nudge_timer_interval_minutes_;
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
      usage_tick_clock_->NowTicks() + nudge_timer_interval_minutes_;
}

}  // namespace tabs
