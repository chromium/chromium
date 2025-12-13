// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_CREATION_METRICS_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_CREATION_METRICS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace tab_groups {
class TabGroupId;
}

namespace tabs {
class TabInterface;

// Enumerates different types of grouping transition a tab undergoes during a
// specific observation window. Each value includes the initial tab grouping
// state and the ending tab grouping state.
// LINT.IfChange(TabGroupingTransitionType)
enum TabGroupingTransitionType {
  // Used when the transition type is not set.
  kUninitialized = 0,

  // The active tab was ungrouped when a new tab was created, and the new tab
  // was left ungrouped.
  kUngroupedToUngrouped = 1,

  // The active tab was ungrouped when a new tab was created, and the new tab
  // was left grouped.
  kUngroupedToGrouped = 2,

  // The active tab was grouped when a new tab was created, and the new tab
  // was left ungrouped.
  kGroupedToUngrouped = 3,

  // The active tab was grouped when a new tab was created, and the new tab
  // was grouped to the existing group of the active tab.
  kGroupedToInPreviousGroup = 4,

  // The active tab was grouped when a new tab was created, and the new tab
  // was grouped to a group different from the active tab's group.
  kGroupedToOutsidePreviousGroup = 5,

  kMaxValue = kGroupedToOutsidePreviousGroup,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/tab/enums.xml:TabGroupingTransitionType)

// Per-tab metrics helper that records how a tab's grouping state changes
// shortly after creation.
//
// When constructed, the controller gets the last active tab group if any, and
// schedules a one-time delayed task on |task_runner_| to log how the tab's
// grouping state changed between creation and the end of the delay (10
// seconds).
class TabCreationMetricsController {
 public:
  static constexpr base::TimeDelta kDelay = base::Seconds(10);

  // Global test hook for setting task runner for the scheduled task. Must be
  // called before TabCreationMetricsController is created. To reset to the
  // current default task runner, pass nullptr to SetTaskRunnerForTesting().
  static void SetTaskRunnerForTesting(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  explicit TabCreationMetricsController(TabInterface* tab);
  ~TabCreationMetricsController();

  TabCreationMetricsController(const TabCreationMetricsController&) = delete;
  TabCreationMetricsController& operator=(const TabCreationMetricsController&) =
      delete;

 private:
  // Posts a delayed task to record the tab's grouping state changes 10 seconds
  // after creation. The histogram will include the tab's initial grouping state
  // and and the ending grouping state.
  void ScheduleRecordTabGroupingTransition();

  void RecordTabGroupingTransition(
      std::optional<tab_groups::TabGroupId> last_active_group_id);

  raw_ptr<TabInterface> tab_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<TabCreationMetricsController> weak_factory_{this};
};
}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_CREATION_METRICS_CONTROLLER_H_
