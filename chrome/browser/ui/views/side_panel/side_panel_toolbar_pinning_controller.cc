// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

SidePanelToolbarPinningController::SidePanelToolbarPinningController(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  Profile* const profile = browser_view_->GetProfile();

  pinned_model_observation_.Observe(PinnedToolbarActionsModel::Get(profile));
  // When the SidePanelPinning feature is enabled observe changes to the
  // pinned actions so we can update the pin button appropriately.
  // TODO(crbug.com/310910098): Observe the PinnedToolbarActionsModel instead
  // when pinned extensions are fully merged into it.
  extensions_model_observation_.Observe(ToolbarActionsModel::Get(profile));
}

SidePanelToolbarPinningController::~SidePanelToolbarPinningController() =
    default;

void SidePanelToolbarPinningController::OnActionsChanged() {
  pin_state_change_observers_.Notify(&Observer::OnPinStateChanged);
}

void SidePanelToolbarPinningController::OnToolbarPinnedActionsChanged() {
  pin_state_change_observers_.Notify(&Observer::OnPinStateChanged);
}

void SidePanelToolbarPinningController::AddObserver(Observer* observer) {
  pin_state_change_observers_.AddObserver(observer);
}

void SidePanelToolbarPinningController::RemoveObserver(Observer* observer) {
  pin_state_change_observers_.RemoveObserver(observer);
}

bool SidePanelToolbarPinningController::GetPinnedStateFor(
    SidePanelEntryKey key) {
  // TODO(crbug.com/310910098): Clean condition up once/if ToolbarActionsModel
  // and PinnedToolbarActionsModel are merged together.
  if (const std::optional<extensions::ExtensionId> extension_id =
          key.extension_id();
      extension_id.has_value()) {
    ToolbarActionsModel* const actions_model =
        ToolbarActionsModel::Get(browser_view_->GetProfile());

    return actions_model->IsActionPinned(*extension_id);
  } else {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser_view_->GetProfile());

    std::optional<actions::ActionId> action_id =
        SidePanelEntryIdToActionId(key.id());
    CHECK(action_id.has_value());
    return actions_model->Contains(action_id.value());
  }
}

void SidePanelToolbarPinningController::UpdatePinState(
    SidePanelEntry::Key entry_key) {
  Profile* const profile = browser_view_->GetProfile();

  std::optional<actions::ActionId> action_id =
      SidePanelUtil::GetActionItem(browser_view_->browser(), entry_key)
          ->GetActionId();
  CHECK(action_id.has_value());

  bool updated_pin_state = false;

  // TODO(crbug.com/310910098): Clean condition up once/if ToolbarActionsModel
  // and PinnedToolbarActionsModel are merged together.
  if (const std::optional<extensions::ExtensionId> extension_id =
          entry_key.extension_id();
      extension_id.has_value()) {
    ToolbarActionsModel* const actions_model =
        ToolbarActionsModel::Get(profile);

    updated_pin_state = !actions_model->IsActionPinned(*extension_id);
    actions_model->SetActionVisibility(*extension_id, updated_pin_state);
  } else {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(profile);

    updated_pin_state = !actions_model->Contains(action_id.value());
    actions_model->UpdatePinnedState(action_id.value(), updated_pin_state);
  }

  SidePanelUtil::RecordPinnedButtonClicked(entry_key.id(), updated_pin_state);
}

void SidePanelToolbarPinningController::UpdateActiveState(
    SidePanelEntryKey key,
    bool show_active_in_toolbar) {
  auto* const toolbar_container =
      browser_view_->toolbar()->pinned_toolbar_actions_container();
  CHECK(toolbar_container);

  // Active extension side-panels have different UI in the toolbar than active
  // built-in side-panels.
  if (key.id() == SidePanelEntryId::kExtension) {
    browser_view_->toolbar()->extensions_container()->UpdateSidePanelState(
        show_active_in_toolbar);
  } else {
    std::optional<actions::ActionId> action_id =
        SidePanelEntryIdToActionId(key.id());
    CHECK(action_id.has_value());
    toolbar_container->UpdateActionState(*action_id, show_active_in_toolbar);
  }
}
