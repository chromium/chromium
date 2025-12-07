// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_PINNING_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_PINNING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

class BrowserView;
class SidePanelEntryKey;

// The SidePanelToolbarPinningController is responsible for updating the pin
// state for a given SidePanelEntry and notifying observers when the pin state
// changes.
class SidePanelToolbarPinningController
    : public PinnedToolbarActionsModel::Observer,
      public ToolbarActionsModel::Observer {
 public:
  explicit SidePanelToolbarPinningController(BrowserView* browser_view);
  ~SidePanelToolbarPinningController() override;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPinStateChanged() = 0;
  };

  // PinnedToolbarActionsModel::Observer:
  void OnActionsChanged() override;

  // ToolbarActionsModel::Observer
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarModelInitialized() override {}
  void OnToolbarPinnedActionsChanged() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if `entry_key` is pinned. False otherwise.
  bool GetPinnedStateFor(SidePanelEntryKey entry_key);

  // Toggles the pin state for `entry_key` when invoked.
  void UpdatePinState(SidePanelEntryKey entry_key);

  void UpdateActiveState(SidePanelEntryKey key, bool show_active_in_toolbar);

 private:
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      extensions_model_observation_{this};

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_model_observation_{this};

  base::ObserverList<Observer> pin_state_change_observers_;
  raw_ptr<BrowserView> browser_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_PINNING_CONTROLLER_H_
