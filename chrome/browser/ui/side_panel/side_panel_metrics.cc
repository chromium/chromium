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

namespace {

std::string_view GetSidePanelNameFor(SidePanelEntry::PanelType panel_type) {
  switch (panel_type) {
    case SidePanelEntry::PanelType::kContent:
      return "SidePanel";
    case SidePanelEntry::PanelType::kToolbar:
      return "SidePanelToolbarHeight";
  }

  NOTREACHED() << "Invalid PanelType " << static_cast<int>(panel_type);
}

std::string_view GetAnimationNameFor(SidePanelAnimationType animation_type) {
  switch (animation_type) {
    case SidePanelAnimationType::kOpen:
      return "Open";
    case SidePanelAnimationType::kOpenWithContentTransition:
      return "OpenWithContentTransition";
    case SidePanelAnimationType::kClose:
      return "Close";
  }

  NOTREACHED() << "Invalid AnimationType " << static_cast<int>(animation_type);
}

}  // namespace

void SidePanelMetrics::RecordSidePanelOpen(
    SidePanelEntry::PanelType type,
    std::optional<SidePanelOpenTrigger> trigger) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({GetSidePanelNameFor(type), ".Show"}).c_str()));

  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({GetSidePanelNameFor(type), ".OpenTrigger"}),
        trigger.value());
  }
}

void SidePanelMetrics::RecordSidePanelShowOrChangeEntryTrigger(
    SidePanelEntry::PanelType type,
    std::optional<SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({GetSidePanelNameFor(type), ".OpenOrChangeEntryTrigger"}),
        trigger.value());
  }
}

void SidePanelMetrics::RecordSidePanelClosed(SidePanelEntry::PanelType type,
                                             base::TimeTicks opened_timestamp) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({GetSidePanelNameFor(type), ".Hide"}).c_str()));

  base::UmaHistogramLongTimes(
      base::StrCat({GetSidePanelNameFor(type), ".OpenDuration"}),
      base::TimeTicks::Now() - opened_timestamp);
}

void SidePanelMetrics::RecordSidePanelResizeMetrics(
    SidePanelEntry::PanelType type,
    SidePanelEntry::Id id,
    int side_panel_contents_width,
    int browser_window_width) {
  std::string_view entry_name = SidePanelEntryIdToHistogramName(id);

  // Metrics per-id and overall for side panel width after resize.
  base::UmaHistogramCounts10000(base::StrCat({GetSidePanelNameFor(type), ".",
                                              entry_name, ".ResizedWidth"}),
                                side_panel_contents_width);
  base::UmaHistogramCounts10000(
      base::StrCat({GetSidePanelNameFor(type), ".ResizedWidth"}),
      side_panel_contents_width);

  // Metrics per-id and overall for side panel width after resize as a
  // percentage of browser width.
  int width_percentage = side_panel_contents_width * 100 / browser_window_width;
  base::UmaHistogramPercentage(
      base::StrCat({GetSidePanelNameFor(type), ".", entry_name,
                    ".ResizedWidthPercentage"}),
      width_percentage);
  base::UmaHistogramPercentage(
      base::StrCat({GetSidePanelNameFor(type), ".ResizedWidthPercentage"}),
      width_percentage);
}

void SidePanelMetrics::RecordNewTabButtonClicked(SidePanelEntry::Id id) {
  base::RecordComputedAction(
      base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                    ".NewTabButtonClicked"}));
}

void SidePanelMetrics::RecordEntryShownMetrics(
    SidePanelEntry::PanelType type,
    SidePanelEntry::Id id,
    base::TimeTicks load_started_timestamp) {
  base::RecordComputedAction(
      base::StrCat({GetSidePanelNameFor(type), ".",
                    SidePanelEntryIdToHistogramName(id), ".Shown"}));
  if (load_started_timestamp != base::TimeTicks()) {
    base::UmaHistogramLongTimes(
        base::StrCat({GetSidePanelNameFor(type), ".",
                      SidePanelEntryIdToHistogramName(id),
                      ".TimeFromEntryTriggerToShown"}),
        base::TimeTicks::Now() - load_started_timestamp);
  }
}

void SidePanelMetrics::RecordEntryHiddenMetrics(
    SidePanelEntry::PanelType type,
    SidePanelEntry::Id id,
    base::TimeTicks shown_timestamp) {
  base::UmaHistogramLongTimes(
      base::StrCat({GetSidePanelNameFor(type), ".",
                    SidePanelEntryIdToHistogramName(id), ".ShownDuration"}),
      base::TimeTicks::Now() - shown_timestamp);
  // To measure extended usage times, Read Anything also needs a higher maximum
  // than what's supported by the standard ShownDuration histogram.
  if (type == SidePanelEntry::PanelType::kContent &&
      id == SidePanelEntryId::kReadAnything) {
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
    SidePanelEntry::PanelType type,
    SidePanelEntry::Id id,
    std::optional<SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({GetSidePanelNameFor(type), ".",
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

void SidePanelMetrics::RecordSidePanelAnimationMetrics(
    SidePanelEntry::PanelType panel_type,
    SidePanelAnimationType animation_type,
    base::TimeDelta largest_step_time,
    int frames_per_second) {
  if (!largest_step_time.is_zero()) {
    base::UmaHistogramTimes(base::StrCat({GetSidePanelNameFor(panel_type),
                                          ".TimeOfLongestAnimationStep"}),
                            largest_step_time);
  }

  if (frames_per_second > 0) {
    base::UmaHistogramCounts100(
        base::StrCat({GetSidePanelNameFor(panel_type), ".",
                      GetAnimationNameFor(animation_type), ".AnimationFPS"}),
        frames_per_second);
  }
}

void SidePanelMetrics::RecordPanelClosedForOtherPanelTypeMetrics(
    SidePanelEntry::PanelType closing_panel_type,
    SidePanelEntry::PanelType opening_panel_type,
    SidePanelEntryId closing_panel_id,
    SidePanelEntryId opening_panel_id) {
  base::RecordComputedAction(
      base::StrCat({GetSidePanelNameFor(closing_panel_type), ".ClosedToOpen.",
                    GetSidePanelNameFor(opening_panel_type)}));
  if (closing_panel_id == SidePanelEntryId::kContextualTasks) {
    base::RecordComputedAction(base::StrCat(
        {GetSidePanelNameFor(closing_panel_type), ".",
         SidePanelEntryIdToHistogramName(closing_panel_id), ".ClosedToOpen.",
         GetSidePanelNameFor(opening_panel_type)}));
    if (opening_panel_id == SidePanelEntryId::kGlic) {
      base::RecordComputedAction(
          base::StrCat({GetSidePanelNameFor(closing_panel_type), ".",
                        SidePanelEntryIdToHistogramName(closing_panel_id),
                        ".EntryClosedToOpen.",
                        SidePanelEntryIdToHistogramName(opening_panel_id)}));
    }
  }
}
