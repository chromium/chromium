// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_helper.h"

#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_coordinator.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "ui/actions/actions.h"

// static
void SidePanelHelper::PopulateGlobalEntries(
    Browser* browser,
    SidePanelRegistry* window_registry) {
  // Add reading list.
  ReadingListSidePanelCoordinator::From(browser)->CreateAndRegisterEntry(
      window_registry);

  // Add bookmarks.
  BookmarksSidePanelCoordinator::From(browser)->CreateAndRegisterEntry(
      window_registry);

  if (webui_browser::IsWebUIBrowserEnabled()) {
    // TODO(webium): Consider supporting additional side panels beyond reading
    // list and bookmarks.
    return;
  }

  // Add tabs from other devices.
  if (TabsFromOtherDevicesSidePanelCoordinator::IsSupported(
          browser->profile())) {
    browser->browser_window_features()
        ->tabs_from_other_devices_side_panel_coordinator()
        ->CreateAndRegisterEntry(window_registry);
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
    HistorySidePanelCoordinator::From(browser)->CreateAndRegisterEntry(
        window_registry);
  }

  // Add comments.
  if (CommentsSidePanelCoordinator::IsSupported()) {
    CommentsSidePanelCoordinator::From(browser)->CreateAndRegisterEntry(
        window_registry);
  }
}

// static
actions::ActionItem* SidePanelHelper::GetActionItem(
    Browser* browser,
    SidePanelEntryKey entry_key) {
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
