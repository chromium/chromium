// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

class BrowserWindowInterface;
class SidePanelEntryScope;
class SidePanelRegistry;

class TabStripModel;
class TabStripModelChange;
struct TabStripSelectionChange;

namespace tab_groups {
class SavedTabGroup;
enum class TriggerSource;
}  // namespace tab_groups

namespace views {
class View;
}

// CommentsSidePanelCoordinator handles the creation and registration of
// the comments SidePanelEntry.
class CommentsSidePanelCoordinator
    : public TabStripModelObserver,
      public tab_groups::TabGroupSyncService::Observer {
 public:
  // TODO(crbug.com/434203413): Remove dependency on BrowserView by implementing
  // a PinnedToolbarActionsController.
  explicit CommentsSidePanelCoordinator(BrowserWindowInterface* browser);
  ~CommentsSidePanelCoordinator() override;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;

  // TabGroupSyncService::Observer
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;

  // Returns whether CommentsSidePanelCoordinator is supported.
  // If this returns false, it should not be registered with the side
  // panel registry.
  static bool IsSupported();

  // Creates and registers the comments side panel entry in the global registry.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  // Creates the comments web view that will be used as the content of the
  // comments side panel entry.
  std::unique_ptr<views::View> CreateCommentsWebView(
      SidePanelEntryScope& scope);

  // Determine if the comments action should be shown in the toolbar for the
  // active tab.
  bool ShouldShowCommentsAction(const tabs::TabInterface* tab);

  // Updates the visuals of the comments action and side panel.
  void UpdateVisuals(const tabs::TabInterface* tab);

  // Updates the visibility of the comments action in the toolbar.
  void UpdateCommentsActionVisibility(bool should_show_comments_action);

  // If the comments side panel is open, temporarily closes it and sets the
  // side_panel_should_be_resumed_ flag.
  void UpdateCommentsSidePanelVisibility(bool should_show_comments_action);

  // Returns the tab group name if the tab is part of a shared tab group,
  // otherwise returns std::nullopt.
  std::optional<std::u16string> GetSharedTabGroupName(
      const tabs::TabInterface* tab);

  // Updates the title of the comments side panel according to the group of the
  // active tab.
  void UpdateSidePanelTitle(std::optional<std::u16string> group_name);

  // Whether the comments side panel was temporarily closed by changing the
  // active tab. When the comments action is shown again, this will be used to
  // restore the side panel.
  bool side_panel_should_be_resumed_ = false;

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_COORDINATOR_H_
