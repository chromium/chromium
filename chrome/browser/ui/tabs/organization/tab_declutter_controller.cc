// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"

#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "content/public/browser/browser_context.h"

namespace tabs {

namespace {
// TODO(b/362269642): Make this constant finch configurable.
constexpr int kStaleThresholdDurationDays = 7;
}  // namespace

TabDeclutterController::TabDeclutterController(
    TabStripModel* tab_strip_model,
    content::BrowserContext* browser_context)
    : stale_tab_threshold_duration_(base::Days(kStaleThresholdDurationDays)),
      tab_strip_model_(tab_strip_model),
      browser_context_(browser_context) {}

TabDeclutterController::~TabDeclutterController() {}

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

  if (DeclutterNudgeCriteriaMet()) {
    for (auto& observer : observers_) {
      observer.OnTriggerDeclutterUIVisibility(!tabs.empty());
    }
  }
}

bool TabDeclutterController::DeclutterNudgeCriteriaMet() {
  // TODO(shibalik): Implement whether the declutter nudge can be shown.
  return true;
}

}  // namespace tabs
