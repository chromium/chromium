// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_

#include <optional>
#include <type_traits>

#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "ui/base/class_property.h"

class Browser;
class SidePanelRegistry;
class SidePanelContentProxy;

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

  // Deregister the entry with the key from the registry and return the view if
  // exists.
  static std::unique_ptr<views::View> DeregisterAndReturnView(
      SidePanelRegistry* registry,
      SidePanelEntry::Key key);

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
      Browser* browser,
      SidePanelEntry::Id id,
      std::optional<SidePanelOpenTrigger> trigger);
  static void RecordComboboxShown();
  static void RecordPinnedButtonClicked(SidePanelEntry::Id id, bool is_pinned);
  static void RecordSidePanelAnimationMetrics(
      base::TimeDelta largest_step_time);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
