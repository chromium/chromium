// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_

#include "base/time/time.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;
class SidePanelRegistry;
class SidePanelContentProxy;
class SidePanelCoordinator;

namespace views {
class View;
}  // namespace views

class SidePanelUtil {
 public:
  using SidePanelOpenTrigger = ::SidePanelOpenTrigger;

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

  static SidePanelCoordinator* GetSidePanelCoordinatorForBrowser(
      Browser* browser);

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
      Browser* browser,
      SidePanelEntry::Id id,
      absl::optional<SidePanelOpenTrigger> trigger);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
