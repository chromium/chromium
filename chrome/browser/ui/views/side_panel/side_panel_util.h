// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_

#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;
class SidePanelRegistry;
class SidePanelContentProxy;

namespace views {
class View;
}  // namespace views

class SidePanelUtil {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SidePanelOpenTrigger {
    kToolbarButton = 0,
    kLensContextMenu = 1,
    kSideSearchPageAction = 2,
    kNotesInPageContextMenu = 3,
    kComboboxSelected = 4,
    kTabChanged = 5,
    kSidePanelEntryDeregistered = 6,
    kIPHSideSearchAutoTrigger = 7,
    kContextMenuSearchOption = 8,
    kMaxValue = kContextMenuSearchOption,
  };

  static void PopulateGlobalEntries(Browser* browser,
                                    SidePanelRegistry* global_registry);

  // Gets the SidePanelContentProxy for the provided view. If one does not
  // exist, this creates one indicating the view is available.
  static SidePanelContentProxy* GetSidePanelContentProxy(
      views::View* content_view);

  static void RecordNewTabButtonClicked(SidePanelEntry::Id id);
  static void RecordSidePanelOpen(absl::optional<SidePanelOpenTrigger> trigger);
  static void RecordSidePanelClosed(base::TimeTicks opened_timestamp);
  static void RecordSidePanelResizeMetrics(SidePanelEntry::Id id,
                                           int side_panel_contents_width,
                                           int browser_window_width);
  static void RecordEntryShownMetrics(SidePanelEntry::Id id);
  static void RecordEntryHiddenMetrics(SidePanelEntry::Id id,
                                       base::TimeTicks shown_timestamp);
  static void RecordEntryShowTriggeredMetrics(
      SidePanelEntry::Id id,
      absl::optional<SidePanelOpenTrigger> trigger);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
