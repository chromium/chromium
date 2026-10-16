// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace tab_groups::saved_tab_groups::metrics {

namespace {

// LINT.IfChange(TabGroupContextMenuSource)
std::string_view GetMetricsSuffixForSubmenuSource(
    SavedTabGroupOpenedSubmenuDesktop source) {
  // These strings are persisted to logs. Entries should not be changed.
  switch (source) {
    case SavedTabGroupOpenedSubmenuDesktop::kBookmarksBar:
      return "BookmarksBar";
    case SavedTabGroupOpenedSubmenuDesktop::kEverythingMenu:
      return "EverythingMenu";
    case SavedTabGroupOpenedSubmenuDesktop::kAppMenu:
      return "AppMenu";
    case SavedTabGroupOpenedSubmenuDesktop::kMacSystemMenu:
      return "MacSystemMenu";
    case SavedTabGroupOpenedSubmenuDesktop::kOrganizerPanel:
      return "OrganizerPanel";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/histograms.xml:TabGroupContextMenuSource)

}  // namespace

void RecordSharedTabGroupRecallType(SharedTabGroupRecallTypeDesktop action) {
  base::UmaHistogramEnumeration("TabGroups.Shared.Recall.Desktop", action);
}

void RecordSharedTabGroupManageType(SharedTabGroupManageTypeDesktop action) {
  base::UmaHistogramEnumeration("TabGroups.Shared.Manage.Desktop", action);
}

void RecordSavedTabGroupOpenedSubmenu(
    SavedTabGroupOpenedSubmenuDesktop submenu_type) {
  base::UmaHistogramEnumeration("TabGroups.SavedTabGroupOpenedSubmenu",
                                submenu_type);
}

void RecordTabGroupContextMenuAction(
    SavedTabGroupOpenedSubmenuDesktop submenu_source,
    TabGroupMenuAction::Type action) {
  base::UmaHistogramEnumeration(
      base::StrCat({"TabGroups.ContextMenu.",
                    GetMetricsSuffixForSubmenuSource(submenu_source),
                    ".Action"}),
      action);
}

void RecordTabGroupEditorBubbleAction(TabGroupEditorBubbleAction action) {
  base::UmaHistogramEnumeration("TabGroups.ContextMenu.EditorBubble.Action",
                                action);
}

}  // namespace tab_groups::saved_tab_groups::metrics
