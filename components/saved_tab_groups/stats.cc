// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"

namespace tab_groups {
namespace stats {
constexpr base::TimeDelta kModifiedThreshold = base::Days(30);

void RecordSavedTabGroupMetrics(SavedTabGroupModel* model) {
  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupCount", model->Count());

  const base::Time current_time = base::Time::Now();

  int pinned_group_count = 0;
  int active_group_count = 0;

  for (const SavedTabGroup& group : model->saved_tab_groups()) {
    base::UmaHistogramCounts10000("TabGroups.SavedTabGroupTabCount",
                                  group.saved_tabs().size());

    const base::TimeDelta duration_saved =
        current_time - group.creation_time_windows_epoch_micros();
    if (!duration_saved.is_negative()) {
      base::UmaHistogramCounts1M("TabGroups.SavedTabGroupAge",
                                 duration_saved.InMinutes());
    }

    const base::TimeDelta duration_since_group_modification =
        current_time - group.update_time_windows_epoch_micros();
    if (!duration_since_group_modification.is_negative()) {
      base::UmaHistogramCounts1M("TabGroups.SavedTabGroupTimeSinceModification",
                                 duration_since_group_modification.InMinutes());

      if (duration_since_group_modification <= kModifiedThreshold) {
        ++active_group_count;
      }
    }

    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      const base::TimeDelta duration_since_tab_modification =
          current_time - tab.update_time_windows_epoch_micros();
      if (duration_since_tab_modification.is_negative()) {
        continue;
      }

      base::UmaHistogramCounts1M(
          "TabGroups.SavedTabGroupTabTimeSinceModification",
          duration_since_tab_modification.InMinutes());
    }

    if (group.is_pinned()) {
      ++pinned_group_count;
    }
  }

  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupPinnedCount",
                                pinned_group_count);
  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupUnpinnedCount",
                                model->Count() - pinned_group_count);
  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupActiveCount",
                                active_group_count);
}

void RecordTabCountMismatchOnConnect(size_t tabs_in_saved_group,
                                     size_t tabs_in_group) {
  if (tabs_in_group > tabs_in_saved_group) {
    base::UmaHistogramCounts100(
        "TabGroups.SavedTabGroups.TabCountDifference.Positive",
        tabs_in_group - tabs_in_saved_group);
  } else if (tabs_in_group < tabs_in_saved_group) {
    base::UmaHistogramCounts100(
        "TabGroups.SavedTabGroups.TabCountDifference.Negative",
        tabs_in_saved_group - tabs_in_group);
  }
}

}  // namespace stats
}  // namespace tab_groups
