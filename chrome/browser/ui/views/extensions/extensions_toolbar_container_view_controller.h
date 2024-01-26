// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_VIEW_CONTROLLER_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

class ExtensionsToolbarContainer;

class ExtensionsToolbarContainerViewController final
    : public TabStripModelObserver,
      public ToolbarActionsModel::Observer,
      public extensions::PermissionsManager::Observer {
 public:
  ExtensionsToolbarContainerViewController(
      Browser* browser,
      ExtensionsToolbarContainer* extensions_container);
  ExtensionsToolbarContainerViewController(
      const ExtensionsToolbarContainerViewController&) = delete;
  const ExtensionsToolbarContainerViewController& operator=(
      const ExtensionsToolbarContainerViewController&) = delete;
  ~ExtensionsToolbarContainerViewController() override;

 private:
  // Maybe displays the In-Product-Help with a specific priority order.
  void MaybeShowIPH();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // PermissionsManager::Observer:
  void OnUserPermissionsSettingsChanged(
      const extensions::PermissionsManager::UserPermissionsSettings& settings)
      override;
  void OnShowAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;
  void OnExtensionDismissedRequests(const extensions::ExtensionId& extension_id,
                                    const url::Origin& origin) override;

  const raw_ptr<Browser> browser_;

  raw_ptr<ExtensionsToolbarContainer> extensions_container_;

  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observation_{this};
  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};
};
#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_VIEW_CONTROLLER_H_
