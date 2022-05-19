// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/feed/feed_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"
#include "components/feed/feed_feature_list.h"
#include "components/user_notes/user_notes_features.h"
#include "ui/accessibility/accessibility_features.h"

// static
void SidePanelUtil::PopulateGlobalEntries(Browser* browser,
                                          SidePanelRegistry* global_registry) {
  // Add reading list.
  ReadingListSidePanelCoordinator::GetOrCreateForBrowser(browser)
      ->CreateAndRegisterEntry(global_registry);

  // Add bookmarks.
  BookmarksSidePanelCoordinator::GetOrCreateForBrowser(browser)
      ->CreateAndRegisterEntry(global_registry);

  // Add history clusters.
  if (base::FeatureList::IsEnabled(features::kSidePanelJourneys) &&
      !browser->profile()->IsIncognitoProfile()) {
    HistoryClustersSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  // Add read anything.
  if (features::IsReadAnythingEnabled()) {
    ReadAnythingCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  // Add user notes.
  if (user_notes::IsUserNotesEnabled()) {
    UserNoteUICoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  // Add feed.
  if (base::FeatureList::IsEnabled(feed::kWebUiFeed)) {
    feed::FeedSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  return;
}

SidePanelContentProxy* SidePanelUtil::GetSidePanelContentProxy(
    views::View* content_view) {
  if (!content_view->GetProperty(kSidePanelContentProxyKey))
    content_view->SetProperty(
        kSidePanelContentProxyKey,
        std::make_unique<SidePanelContentProxy>(true).release());
  return content_view->GetProperty(kSidePanelContentProxyKey);
}
