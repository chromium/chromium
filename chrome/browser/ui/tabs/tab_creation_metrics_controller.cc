// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_creation_metrics_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/new_tab_grouping_user_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"

namespace {
scoped_refptr<base::SequencedTaskRunner>& GetTaskRunnerOverride() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner;
  return *task_runner;
}
}  // namespace

namespace tabs {

void TabCreationMetricsController::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  GetTaskRunnerOverride() = task_runner;
}

TabCreationMetricsController::TabCreationMetricsController(TabInterface* tab)
    : tab_(tab),
      task_runner_(GetTaskRunnerOverride()
                       ? GetTaskRunnerOverride()
                       : base::SequencedTaskRunner::GetCurrentDefault()) {
  ScheduleRecordTabGroupingTransition();
}

TabCreationMetricsController::~TabCreationMetricsController() = default;

void TabCreationMetricsController::ScheduleRecordTabGroupingTransition() {
  TabStripModel* tab_strip_model =
      tab_->GetBrowserWindowInterface()->GetTabStripModel();
  if (!tab_strip_model) {
    return;
  }

  std::optional<tab_groups::TabGroupId> last_active_group_id;
  if (Profile* profile = tab_strip_model->profile()) {
    if (base::SupportsUserData::Data* data = profile->GetUserData(
            NewTabGroupingUserData::kNewTabGroupingUserDataKey)) {
      last_active_group_id =
          static_cast<NewTabGroupingUserData*>(data)->last_active_group_id();
    }
  }

  if (!last_active_group_id.has_value()) {
    last_active_group_id = tab_->GetBrowserWindowInterface()
                               ->GetTabStripModel()
                               ->GetActiveTabGroupId();
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabCreationMetricsController::RecordTabGroupingTransition,
                     weak_factory_.GetWeakPtr(), last_active_group_id),
      kDelay);
}

void TabCreationMetricsController::RecordTabGroupingTransition(
    std::optional<tab_groups::TabGroupId> last_active_group_id) {
  if (!tab_) {
    return;
  }

  TabStripModel* tab_strip_model =
      tab_->GetBrowserWindowInterface()->GetTabStripModel();
  int tab_index = tab_strip_model->GetIndexOfTab(tab_);
  std::optional<tab_groups::TabGroupId> group_id =
      tab_->GetBrowserWindowInterface()->GetTabStripModel()->GetTabGroupForTab(
          tab_index);

  TabGroupingTransitionType type = TabGroupingTransitionType::kUninitialized;
  if (!last_active_group_id.has_value() && !group_id.has_value()) {
    type = TabGroupingTransitionType::kUngroupedToUngrouped;
  } else if (!last_active_group_id.has_value() && group_id.has_value()) {
    type = TabGroupingTransitionType::kUngroupedToGrouped;
  } else if (last_active_group_id.has_value() && !group_id.has_value()) {
    type = TabGroupingTransitionType::kGroupedToUngrouped;
  } else {
    // last_active_group_id.has_value() && group_id.has_value()
    if (last_active_group_id.value() == group_id.value()) {
      type = TabGroupingTransitionType::kGroupedToInPreviousGroup;
    } else {
      type = TabGroupingTransitionType::kGroupedToOutsidePreviousGroup;
    }
  }
  base::UmaHistogramEnumeration("Tab.GroupingTransition2", type);
}

}  // namespace tabs
