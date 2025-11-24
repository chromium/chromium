// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/common/chrome_features.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/actions.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_legacy_side_panel_coordinator.h"
#endif

namespace {

std::string GetSidePanelNameFor(SidePanelEntry::PanelType type) {
  return type == SidePanelEntry::PanelType::kContent ? "SidePanel"
                                                     : "SidePanelToolbarHeight";
}

}  // namespace

// static
void SidePanelUtil::PopulateGlobalEntries(Browser* browser,
                                          SidePanelRegistry* window_registry) {
  // Add reading list.
  browser->browser_window_features()
      ->reading_list_side_panel_coordinator()
      ->CreateAndRegisterEntry(window_registry);

  // Add bookmarks.
  browser->browser_window_features()
      ->bookmarks_side_panel_coordinator()
      ->CreateAndRegisterEntry(window_registry);

  if (webui_browser::IsWebUIBrowserEnabled()) {
    // TODO(webium): Consider supporting additional side panels beyond reading
    // list and bookmarks.
    return;
  }

  // Add history clusters.
  if (HistoryClustersSidePanelCoordinator::IsSupported(browser->profile()) &&
      !HistorySidePanelCoordinator::IsSupported()) {
    browser->GetFeatures()
        .history_clusters_side_panel_coordinator()
        ->CreateAndRegisterEntry(window_registry);
  }

  // Add history.
  if (HistorySidePanelCoordinator::IsSupported()) {
    browser->browser_window_features()
        ->history_side_panel_coordinator()
        ->CreateAndRegisterEntry(window_registry);
  }

  // Add comments.
  if (CommentsSidePanelCoordinator::IsSupported()) {
    browser->browser_window_features()
        ->comments_side_panel_coordinator()
        ->CreateAndRegisterEntry(window_registry);
  }
#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsEnabledForProfile(browser->profile()) &&
      browser->is_type_normal() &&
      !glic::GlicEnabling::IsMultiInstanceEnabled()) {
    browser->browser_window_features()
        ->glic_side_panel_coordinator()
        ->CreateAndRegisterEntry(browser, window_registry);
  }
#endif
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

actions::ActionItem* SidePanelUtil::GetActionItem(
    Browser* browser,
    SidePanelEntry::Key entry_key) {
  BrowserActions* const browser_actions = browser->browser_actions();
  if (entry_key.id() == SidePanelEntryId::kExtension) {
    std::optional<actions::ActionId> extension_action_id =
        actions::ActionIdMap::StringToActionId(entry_key.ToString());
    CHECK(extension_action_id.has_value());
    actions::ActionItem* const action_item =
        actions::ActionManager::Get().FindAction(
            extension_action_id.value(), browser_actions->root_action_item());
    CHECK(action_item);
    return action_item;
  }

  std::optional<actions::ActionId> action_id =
      SidePanelEntryIdToActionId(entry_key.id());
  CHECK(action_id.has_value());
  return actions::ActionManager::Get().FindAction(
      action_id.value(), browser_actions->root_action_item());
}

void SidePanelUtil::RecordSidePanelOpen(
    SidePanelEntry::PanelType type,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({GetSidePanelNameFor(type), ".Show"}).c_str()));

  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({GetSidePanelNameFor(type), ".OpenTrigger"}),
        trigger.value());
  }
}

void SidePanelUtil::RecordSidePanelShowOrChangeEntryTrigger(
    SidePanelEntry::PanelType type,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({GetSidePanelNameFor(type), ".OpenOrChangeEntryTrigger"}),
        trigger.value());
  }
}

void SidePanelUtil::RecordSidePanelClosed(SidePanelEntry::PanelType type,
                                          base::TimeTicks opened_timestamp) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({GetSidePanelNameFor(type), ".Hide"}).c_str()));

  base::UmaHistogramLongTimes(
      base::StrCat({GetSidePanelNameFor(type), ".OpenDuration"}),
      base::TimeTicks::Now() - opened_timestamp);
}

void SidePanelUtil::RecordSidePanelResizeMetrics(SidePanelEntry::PanelType type,
                                                 SidePanelEntry::Id id,
                                                 int side_panel_contents_width,
                                                 int browser_window_width) {
  std::string entry_name = SidePanelEntryIdToHistogramName(id);

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

void SidePanelUtil::RecordNewTabButtonClicked(SidePanelEntry::Id id) {
  base::RecordComputedAction(
      base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                    ".NewTabButtonClicked"}));
}

void SidePanelUtil::RecordEntryShownMetrics(
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

void SidePanelUtil::RecordEntryHiddenMetrics(SidePanelEntry::PanelType type,
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

void SidePanelUtil::RecordEntryShowTriggeredMetrics(
    SidePanelEntry::PanelType type,
    Browser* browser,
    SidePanelEntry::Id id,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({GetSidePanelNameFor(type), ".",
                      SidePanelEntryIdToHistogramName(id), ".ShowTriggered"}),
        trigger.value());
  }
}

void SidePanelUtil::RecordPinnedButtonClicked(SidePanelEntry::Id id,
                                              bool is_pinned) {
  base::RecordComputedAction(base::StrCat(
      {"SidePanel.", SidePanelEntryIdToHistogramName(id), ".",
       is_pinned ? "Pinned" : "Unpinned", ".BySidePanelHeaderButton"}));
}

void SidePanelUtil::RecordSidePanelAnimationMetrics(
    SidePanelEntry::PanelType type,
    base::TimeDelta largest_step_time) {
  base::UmaHistogramTimes(
      base::StrCat({GetSidePanelNameFor(type), ".TimeOfLongestAnimationStep"}),
      largest_step_time);
}
