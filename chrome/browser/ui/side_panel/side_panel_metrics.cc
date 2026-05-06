// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/side_panel/side_panel_enums_utils.h"

void SidePanelMetrics::RecordSidePanelOpen(
    std::optional<SidePanelOpenTrigger> trigger) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({kSidePanelHistogramName, ".Show"}).c_str()));

  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({kSidePanelHistogramName, ".OpenTrigger"}),
        trigger.value());
  }
}

void SidePanelMetrics::RecordSidePanelShowOrChangeEntryTrigger(
    std::optional<SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({kSidePanelHistogramName, ".OpenOrChangeEntryTrigger"}),
        trigger.value());
  }
}

void SidePanelMetrics::RecordSidePanelClosed(base::TimeTicks opened_timestamp) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({kSidePanelHistogramName, ".Hide"}).c_str()));

  base::UmaHistogramLongTimes(
      base::StrCat({kSidePanelHistogramName, ".OpenDuration"}),
      base::TimeTicks::Now() - opened_timestamp);
}

void SidePanelMetrics::RecordSidePanelResizeMetrics(
    SidePanelEntry::Id id,
    int side_panel_contents_width,
    int browser_window_width) {
  std::string_view entry_name = SidePanelEntryIdToHistogramName(id);

  // Metrics per-id and overall for side panel width after resize.
  base::UmaHistogramCounts10000(
      base::StrCat({kSidePanelHistogramName, ".", entry_name, ".ResizedWidth"}),
      side_panel_contents_width);
  base::UmaHistogramCounts10000(
      base::StrCat({kSidePanelHistogramName, ".ResizedWidth"}),
      side_panel_contents_width);

  // Metrics per-id and overall for side panel width after resize as a
  // percentage of browser width.
  int width_percentage = side_panel_contents_width * 100 / browser_window_width;
  base::UmaHistogramPercentage(
      base::StrCat({kSidePanelHistogramName, ".", entry_name,
                    ".ResizedWidthPercentage"}),
      width_percentage);
  base::UmaHistogramPercentage(
      base::StrCat({kSidePanelHistogramName, ".ResizedWidthPercentage"}),
      width_percentage);
}

void SidePanelMetrics::RecordNewTabButtonClicked(SidePanelEntry::Id id) {
  base::RecordComputedAction(
      base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                    ".NewTabButtonClicked"}));
}

void SidePanelMetrics::RecordEntryShownMetrics(
    SidePanelEntry::Id id,
    base::TimeTicks load_started_timestamp) {
  base::RecordComputedAction(
      base::StrCat({kSidePanelHistogramName, ".",
                    SidePanelEntryIdToHistogramName(id), ".Shown"}));
  if (load_started_timestamp != base::TimeTicks()) {
    base::UmaHistogramLongTimes(
        base::StrCat({kSidePanelHistogramName, ".",
                      SidePanelEntryIdToHistogramName(id),
                      ".TimeFromEntryTriggerToShown"}),
        base::TimeTicks::Now() - load_started_timestamp);
  }
}

void SidePanelMetrics::RecordEntryHiddenMetrics(
    SidePanelEntry::Id id,
    base::TimeTicks shown_timestamp) {
  base::UmaHistogramLongTimes(
      base::StrCat({kSidePanelHistogramName, ".",
                    SidePanelEntryIdToHistogramName(id), ".ShownDuration"}),
      base::TimeTicks::Now() - shown_timestamp);
  // To measure extended usage times, Read Anything also needs a higher maximum
  // than what's supported by the standard ShownDuration histogram.
  if (id == SidePanelEntryId::kReadAnything) {
    // TODO(crbug.com/456824119): Consider removing the standard ShownDuration
    // histogram for Read Anything after this one has gathered enough data.
    base::UmaHistogramCustomTimes("SidePanel.ReadAnything.ShownDurationMax1Day",
                                  base::TimeTicks::Now() - shown_timestamp,
                                  /*min=*/base::Seconds(1),
                                  /*max=*/base::Hours(24),
                                  /*buckets=*/100);
  }
}

void SidePanelMetrics::RecordEntryShowTriggeredMetrics(
    SidePanelEntry::Id id,
    std::optional<SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({kSidePanelHistogramName, ".",
                      SidePanelEntryIdToHistogramName(id), ".ShowTriggered"}),
        trigger.value());
  }
}

void SidePanelMetrics::RecordPinnedButtonClicked(SidePanelEntry::Id id,
                                                 bool is_pinned) {
  base::RecordComputedAction(base::StrCat(
      {"SidePanel.", SidePanelEntryIdToHistogramName(id), ".",
       is_pinned ? "Pinned" : "Unpinned", ".BySidePanelHeaderButton"}));
}

void SidePanelMetrics::RecordPanelClosedForOtherPanelTypeMetrics(
    SidePanelEntryId closing_panel_id,
    SidePanelEntryId opening_panel_id) {
  if (closing_panel_id == SidePanelEntryId::kContextualTasks &&
      opening_panel_id == SidePanelEntryId::kGlic) {
    base::RecordComputedAction(
        base::StrCat({kSidePanelHistogramName, ".",
                      SidePanelEntryIdToHistogramName(closing_panel_id),
                      ".EntryClosedToOpen.",
                      SidePanelEntryIdToHistogramName(opening_panel_id)}));
  }
}
