// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_METRICS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"

class SidePanelMetrics {
 public:
  // LINT.IfChange(SidePanelType)
  static constexpr char kSidePanelHistogramName[] = "SidePanel";
  static constexpr char kSidePanelToolbarHeightHistogramName[] =
      "SidePanelToolbarHeight";
  // LINT.ThenChange(//tools/metrics/histograms/metadata/browser/histograms.xml:SidePanelType)

  static void RecordNewTabButtonClicked(SidePanelEntry::Id id);
  static void RecordSidePanelOpen(std::optional<SidePanelOpenTrigger> trigger);
  static void RecordSidePanelShowOrChangeEntryTrigger(
      std::optional<SidePanelOpenTrigger> trigger);
  static void RecordSidePanelClosed(base::TimeTicks opened_timestamp);
  static void RecordSidePanelResizeMetrics(SidePanelEntry::Id id,
                                           int side_panel_contents_width,
                                           int browser_window_width);
  static void RecordEntryShownMetrics(SidePanelEntry::Id id,
                                      base::TimeTicks load_started_timestamp);
  static void RecordEntryHiddenMetrics(SidePanelEntry::Id id,
                                       base::TimeTicks shown_timestamp);
  static void RecordEntryShowTriggeredMetrics(
      SidePanelEntry::Id id,
      std::optional<SidePanelOpenTrigger> trigger);
  static void RecordPinnedButtonClicked(SidePanelEntry::Id id, bool is_pinned);
  static void RecordPanelClosedForOtherPanelTypeMetrics(
      SidePanelEntryId closing_panel_id,
      SidePanelEntryId opening_panel_id);
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_METRICS_H_
