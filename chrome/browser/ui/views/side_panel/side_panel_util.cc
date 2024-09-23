// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_notes/user_notes_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/actions.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"

// static
void SidePanelUtil::PopulateGlobalEntries(Browser* browser,
                                          SidePanelRegistry* window_registry) {
  // Add reading list.
  ReadingListSidePanelCoordinator::GetOrCreateForBrowser(browser)
      ->CreateAndRegisterEntry(window_registry);

  // Add bookmarks.
  BookmarksSidePanelCoordinator::GetOrCreateForBrowser(browser)
      ->CreateAndRegisterEntry(window_registry);

  // Add history clusters.
  if (HistoryClustersSidePanelCoordinator::IsSupported(browser->profile())) {
    HistoryClustersSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(window_registry);
  }

  // Create Search Companion coordinator.
  // Disable runtime checks so that coordinator can monitor the runtime changes
  // in the availability of companion.
  if (companion::IsCompanionFeatureEnabled() &&
      SearchCompanionSidePanelCoordinator::IsSupported(
          browser->profile(),
          /*include_runtime_checks=*/false)) {
    SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
  }
}

SidePanelContentProxy* SidePanelUtil::GetSidePanelContentProxy(
    views::View* content_view) {
  if (!content_view->GetProperty(kSidePanelContentProxyKey)) {
    content_view->SetProperty(
        kSidePanelContentProxyKey,
        std::make_unique<SidePanelContentProxy>(true).release());
  }
  return content_view->GetProperty(kSidePanelContentProxyKey);
}

std::unique_ptr<views::View> SidePanelUtil::DeregisterAndReturnView(
    SidePanelRegistry* registry,
    SidePanelEntry::Key key) {
  std::unique_ptr<SidePanelEntry> entry =
      registry->DeregisterAndReturnEntry(key);
  return entry->CachedView() ? entry->GetContent() : nullptr;
}

void SidePanelUtil::RecordSidePanelOpen(
    std::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  base::RecordAction(base::UserMetricsAction("SidePanel.Show"));

  if (trigger.has_value()) {
    base::UmaHistogramEnumeration("SidePanel.OpenTrigger", trigger.value());
  }
}

void SidePanelUtil::RecordSidePanelShowOrChangeEntryTrigger(
    std::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration("SidePanel.OpenOrChangeEntryTrigger",
                                  trigger.value());
  }
}

void SidePanelUtil::RecordSidePanelClosed(base::TimeTicks opened_timestamp) {
  base::RecordAction(base::UserMetricsAction("SidePanel.Hide"));

  base::UmaHistogramLongTimes("SidePanel.OpenDuration",
                              base::TimeTicks::Now() - opened_timestamp);
}

void SidePanelUtil::RecordSidePanelResizeMetrics(SidePanelEntry::Id id,
                                                 int side_panel_contents_width,
                                                 int browser_window_width) {
  std::string entry_name = SidePanelEntryIdToHistogramName(id);

  // Metrics per-id and overall for side panel width after resize.
  base::UmaHistogramCounts10000(
      base::StrCat({"SidePanel.", entry_name, ".ResizedWidth"}),
      side_panel_contents_width);
  base::UmaHistogramCounts10000("SidePanel.ResizedWidth",
                                side_panel_contents_width);

  // Metrics per-id and overall for side panel width after resize as a
  // percentage of browser width.
  int width_percentage = side_panel_contents_width * 100 / browser_window_width;
  base::UmaHistogramPercentage(
      base::StrCat({"SidePanel.", entry_name, ".ResizedWidthPercentage"}),
      width_percentage);
  base::UmaHistogramPercentage("SidePanel.ResizedWidthPercentage",
                               width_percentage);
}

void SidePanelUtil::RecordNewTabButtonClicked(SidePanelEntry::Id id) {
  base::RecordComputedAction(
      base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                    ".NewTabButtonClicked"}));
}

void SidePanelUtil::RecordEntryShownMetrics(
    SidePanelEntry::Id id,
    base::TimeTicks load_started_timestamp) {
  base::RecordComputedAction(base::StrCat(
      {"SidePanel.", SidePanelEntryIdToHistogramName(id), ".Shown"}));
  if (load_started_timestamp != base::TimeTicks()) {
    base::UmaHistogramLongTimes(
        base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                      ".TimeFromEntryTriggerToShown"}),
        base::TimeTicks::Now() - load_started_timestamp);
  }
}

void SidePanelUtil::RecordEntryHiddenMetrics(SidePanelEntry::Id id,
                                             base::TimeTicks shown_timestamp) {
  base::UmaHistogramLongTimes(
      base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                    ".ShownDuration"}),
      base::TimeTicks::Now() - shown_timestamp);
}

void SidePanelUtil::RecordEntryShowTriggeredMetrics(
    Browser* browser,
    SidePanelEntry::Id id,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                      ".ShowTriggered"}),
        trigger.value());
  }

  if (id == SidePanelEntry::Id::kSearchCompanion) {
    auto* search_companion_coordinator =
        SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
    search_companion_coordinator->NotifyCompanionOfSidePanelOpenTrigger(
        trigger);
  }
}

void SidePanelUtil::RecordComboboxShown() {
  base::UmaHistogramBoolean("SidePanel.ComboboxMenuShown", true);
}

void SidePanelUtil::RecordPinnedButtonClicked(SidePanelEntry::Id id,
                                              bool is_pinned) {
  base::RecordComputedAction(base::StrCat(
      {"SidePanel.", SidePanelEntryIdToHistogramName(id), ".",
       is_pinned ? "Pinned" : "Unpinned", ".BySidePanelHeaderButton"}));
}

void SidePanelUtil::RecordSidePanelAnimationMetrics(
    base::TimeDelta largest_step_time) {
  base::UmaHistogramTimes("SidePanel.TimeOfLongestAnimationStep",
                          largest_step_time);
}
