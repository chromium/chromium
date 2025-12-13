// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_

#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

// ViewModel for the ExtensionsToolbarContainer. This class manages the business
// logic for the order and state of extension actions in the toolbar. It serves
// as the single source of truth for the ordering of the list of actions.
class ExtensionsToolbarViewModel {
 public:
  ExtensionsToolbarViewModel();
  ExtensionsToolbarViewModel(const ExtensionsToolbarViewModel&) = delete;
  ExtensionsToolbarViewModel& operator=(ExtensionsToolbarViewModel&) = delete;
  ~ExtensionsToolbarViewModel();

  // TODO(crbug.com/461981075): This is temporary for the refactor. We should
  // only expose attributes that are necessary.
  const std::vector<std::unique_ptr<ToolbarActionViewModel>>& GetActions() {
    return actions_;
  }

  // Adds the action view model corresponding to `action_id` to the list of
  // actions.
  void AddAction(
      const ToolbarActionsModel::ActionId& action_id,
      BrowserWindowInterface* browser,
      std::unique_ptr<ExtensionActionPlatformDelegate> platform_delegate);

  // Removes the action view model corresponding to `action_id` from list of
  // actions. The returning pointer can be used to ensure that the action
  // outlives the UI element for cleanup.
  std::unique_ptr<ToolbarActionViewModel> RemoveAction(
      const ToolbarActionsModel::ActionId& action_id);

 private:
  // TODO(crbug.com/461981075): Use the order of this vector as the
  // source of truth for action view order.
  // Actions for all extensions.
  std::vector<std::unique_ptr<ToolbarActionViewModel>> actions_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_
