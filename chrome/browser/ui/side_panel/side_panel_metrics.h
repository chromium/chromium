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
  static void RecordNewTabButtonClicked(SidePanelEntry::Id id);
  static void RecordSidePanelOpen(SidePanelType type,
                                  std::optional<SidePanelOpenTrigger> trigger);
  static void RecordSidePanelShowOrChangeEntryTrigger(
      SidePanelType type,
      std::optional<SidePanelOpenTrigger> trigger);
  static void RecordSidePanelClosed(SidePanelType type,
                                    base::TimeTicks opened_timestamp);
  static void RecordSidePanelResizeMetrics(SidePanelType type,
                                           SidePanelEntry::Id id,
                                           int side_panel_contents_width,
                                           int browser_window_width);
  static void RecordEntryShownMetrics(SidePanelType type,
                                      SidePanelEntry::Id id,
                                      base::TimeTicks load_started_timestamp);
  static void RecordEntryHiddenMetrics(SidePanelType type,
                                       SidePanelEntry::Id id,
                                       base::TimeTicks shown_timestamp);
  static void RecordEntryShowTriggeredMetrics(
      SidePanelType type,
      SidePanelEntry::Id id,
      std::optional<SidePanelOpenTrigger> trigger);
  static void RecordPinnedButtonClicked(SidePanelEntry::Id id, bool is_pinned);
  static void RecordSidePanelAnimationMetrics(
      SidePanelType panel_type,
      SidePanelAnimationType animation_type,
      base::TimeDelta largest_step_time,
      int frames_per_second);
  static void RecordPanelClosedForOtherPanelTypeMetrics(
      SidePanelType closing_panel_type,
      SidePanelType opening_panel_type,
      SidePanelEntryId closing_panel_id,
      SidePanelEntryId opening_panel_id);
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_METRICS_H_
