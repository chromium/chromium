// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_

#include <optional>
#include <type_traits>

#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "ui/base/class_property.h"

class Browser;
class SidePanelRegistry;
class SidePanelContentProxy;

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class View;
}  // namespace views

class SidePanelUtil {
 public:
  using SidePanelOpenTrigger = ::SidePanelOpenTrigger;
  using SidePanelContentState = ::SidePanelContentState;

  static void PopulateGlobalEntries(Browser* browser,
                                    SidePanelRegistry* global_registry);

  // Gets the SidePanelContentProxy for the provided view. If one does not
  // exist, this creates one indicating the view is available.
  static SidePanelContentProxy* GetSidePanelContentProxy(
      views::View* content_view);

  static actions::ActionItem* GetActionItem(Browser* browser,
                                            SidePanelEntryKey entry_key);

  static void RecordNewTabButtonClicked(SidePanelEntry::Id id);
  static void RecordSidePanelOpen(SidePanelEntry::PanelType type,
                                  std::optional<SidePanelOpenTrigger> trigger);
  static void RecordSidePanelShowOrChangeEntryTrigger(
      SidePanelEntry::PanelType type,
      std::optional<SidePanelOpenTrigger> trigger);
  static void RecordSidePanelClosed(SidePanelEntry::PanelType type,
                                    base::TimeTicks opened_timestamp);
  static void RecordSidePanelResizeMetrics(SidePanelEntry::PanelType type,
                                           SidePanelEntry::Id id,
                                           int side_panel_contents_width,
                                           int browser_window_width);
  static void RecordEntryShownMetrics(SidePanelEntry::PanelType type,
                                      SidePanelEntry::Id id,
                                      base::TimeTicks load_started_timestamp);
  static void RecordEntryHiddenMetrics(SidePanelEntry::PanelType type,
                                       SidePanelEntry::Id id,
                                       base::TimeTicks shown_timestamp);
  static void RecordEntryShowTriggeredMetrics(
      SidePanelEntry::PanelType type,
      Browser* browser,
      SidePanelEntry::Id id,
      std::optional<SidePanelOpenTrigger> trigger);
  static void RecordPinnedButtonClicked(SidePanelEntry::Id id, bool is_pinned);
  static void RecordSidePanelAnimationMetrics(
      SidePanelEntry::PanelType type,
      base::TimeDelta largest_step_time);
  static void RecordPanelClosedForOtherPanelTypeMetrics(
      SidePanelEntry::PanelType closing_panel_type,
      SidePanelEntry::PanelType opening_panel_type,
      SidePanelEntryId closing_panel_id,
      SidePanelEntryId opening_panel_id);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
