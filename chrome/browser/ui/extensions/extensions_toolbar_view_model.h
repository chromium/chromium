// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_

#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

// ViewModel for the ExtensionsToolbarContainer. This class manages the business
// logic for the order and state of extension actions in the toolbar. It serves
// as the single source of truth for the ordering of the list of actions.
class ExtensionsToolbarViewModel : public ToolbarActionsModel::Observer {
 public:
  // Delegate used to retrieve platform-specific information.
  class Delegate {
   public:
    // Creates the platform-specific action view model.
    virtual std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
        const ToolbarActionsModel::ActionId& action_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Observer used to notify platforms about changes to the model.
  class Observer : public base::CheckedObserver {
   public:
    // Called after all actions are added to the model.
    virtual void OnActionsInitialized() = 0;

    // Called when an action is added to the model.
    virtual void OnActionAdded(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called when an action is removed from the model.
    virtual void OnActionRemoved(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called when an action in the model is updated.
    virtual void OnActionUpdated(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called when the pinned actions in the model are changed.
    virtual void OnPinnedActionsChanged() = 0;
  };

  ExtensionsToolbarViewModel(Delegate* delegate,
                             ToolbarActionsModel* actions_model);
  ExtensionsToolbarViewModel(const ExtensionsToolbarViewModel&) = delete;
  ExtensionsToolbarViewModel& operator=(ExtensionsToolbarViewModel&) = delete;
  ~ExtensionsToolbarViewModel() override;

  const std::vector<std::unique_ptr<ToolbarActionViewModel>>& GetActions() {
    return actions_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether the actions are initialized.
  bool AreActionsInitialized();

  // ToolbarActionsModel::Observer:
  void OnToolbarModelInitialized() override;
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarPinnedActionsChanged() override;

 private:
  // Creates and appends an action model to `actions_` vector.
  void AppendActionModel(const ToolbarActionsModel::ActionId& action_id);

  // The delegate to retrieve platform-specific information.
  const raw_ptr<Delegate> delegate_;

  const raw_ptr<ToolbarActionsModel> actions_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      actions_model_observation_{this};

  // The observers that handles platform-specific UI.
  base::ObserverList<Observer> observers_;

  // Actions for all extensions.
  std::vector<std::unique_ptr<ToolbarActionViewModel>> actions_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_
