// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_metrics.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_helper.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view_utils.h"

SidePanelToolbarPinningController::SidePanelToolbarPinningController(
    BrowserWindowInterface* browser)
    : browser_(CHECK_DEREF(browser)) {
  Profile* const profile = browser_->GetProfile();

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
  ReevaluateActiveState();
}

void SidePanelToolbarPinningController::OnToolbarPinnedActionsChanged() {
  pin_state_change_observers_.Notify(&Observer::OnPinStateChanged);
  ReevaluateActiveState();
}

void SidePanelToolbarPinningController::ReevaluateActiveState() {
  if (auto* side_panel_ui = SidePanelUI::From(&*browser_)) {
    if (std::optional<SidePanelEntryId> current_id =
            side_panel_ui->GetCurrentEntryId()) {
      if (current_id == SidePanelEntryId::kExtension) {
        return;
      }
      SidePanelEntryKey key(*current_id);
      SidePanelEntry* entry = nullptr;
      if (auto* global_registry = SidePanelRegistry::From(&*browser_)) {
        entry = global_registry->GetEntryForKey(key);
      }
      if (!entry) {
        if (auto* tab_interface = browser_->GetActiveTabInterface()) {
          if (auto* tab_registry = SidePanelRegistry::From(tab_interface)) {
            entry = tab_registry->GetEntryForKey(key);
          }
        }
      }
      if (entry) {
        UpdateActiveState(key, ShouldShowActiveInToolbar(entry));
      }
    }
  }
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
        ToolbarActionsModel::Get(browser_->GetProfile());

    return actions_model->IsActionPinned(*extension_id);
  } else {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser_->GetProfile());

    std::optional<actions::ActionId> action_id =
        SidePanelEntryIdToActionId(key.id());
    CHECK(action_id.has_value());
    return actions_model->Contains(action_id.value());
  }
}

void SidePanelToolbarPinningController::UpdatePinState(
    SidePanelEntry::Key entry_key) {
  Profile* const profile = browser_->GetProfile();

  std::optional<actions::ActionId> action_id =
      SidePanelHelper::GetActionItem(&*browser_, entry_key)->GetActionId();
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
    if (updated_pin_state) {
      ui::TrackedElement* browser_element =
          ui::ElementTracker::GetElementTracker()->GetUniqueElement(
              kBrowserViewElementId,
              BrowserElements::From(&*browser_)->GetContext());
      if (browser_element) {
        ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
            browser_element, kExtensionsSidePanelPinExtensionsEventId);
      }
    }
  } else {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(profile);

    updated_pin_state = !actions_model->Contains(action_id.value());
    actions_model->UpdatePinnedState(action_id.value(), updated_pin_state);
  }

  SidePanelMetrics::RecordPinnedButtonClicked(entry_key.id(),
                                              updated_pin_state);
}

bool SidePanelToolbarPinningController::ShouldShowActiveInToolbar(
    const SidePanelEntry* entry) {
  if (entry && entry->key().id() == SidePanelEntryId::kContextualTasks) {
    auto* contextual_tasks_controller =
        contextual_tasks::ContextualTasksPanelController::From(&*browser_);
    if (contextual_tasks_controller &&
        contextual_tasks_controller->GetActiveEntrySource() ==
            contextual_tasks::ContextualTasksPanelController::EntrySource::
                kLensOverlay) {
      return true;
    }
  }
  return entry && (entry->should_show_ephemerally_in_toolbar() ||
                   GetPinnedStateFor(entry->key()));
}

void SidePanelToolbarPinningController::UpdateActiveState(
    SidePanelEntryKey key,
    bool show_active_in_toolbar) {
  auto* const toolbar_container =
      ToolbarButtonProvider::From(&*browser_)->GetPinnedToolbarActions();
  CHECK(toolbar_container);

  // Active extension side-panels have different UI in the toolbar than active
  // built-in side-panels.
  if (key.id() == SidePanelEntryId::kExtension) {
    if (auto* extensions_container =
            views::AsViewClass<ExtensionsToolbarDesktop>(
                BrowserElementsViews::From(&*browser_)
                    ->GetView(kToolbarExtensionsContainerElementId))) {
      extensions_container->UpdateSidePanelState(show_active_in_toolbar);
    }
  } else {
    SidePanelEntryId target_id = key.id();
    std::optional<SidePanelEntryId> other_id;

    if (target_id == SidePanelEntryId::kContextualTasks) {
      auto* contextual_tasks_controller =
          contextual_tasks::ContextualTasksPanelController::From(&*browser_);
      if (contextual_tasks_controller &&
          contextual_tasks_controller->GetActiveEntrySource() ==
              contextual_tasks::ContextualTasksPanelController::EntrySource::
                  kLensOverlay) {
        target_id = SidePanelEntryId::kLensOverlayResults;
        other_id = SidePanelEntryId::kContextualTasks;
      } else {
        other_id = SidePanelEntryId::kLensOverlayResults;
      }
    } else if (target_id == SidePanelEntryId::kLensOverlayResults) {
      other_id = SidePanelEntryId::kContextualTasks;
    }

    std::optional<actions::ActionId> action_id =
        SidePanelEntryIdToActionId(target_id);
    CHECK(action_id.has_value());
    toolbar_container->UpdateActionState(*action_id, show_active_in_toolbar);

    if (other_id.has_value()) {
      std::optional<actions::ActionId> other_action_id =
          SidePanelEntryIdToActionId(*other_id);
      CHECK(other_action_id.has_value());
      toolbar_container->UpdateActionState(*other_action_id, false);
    }
  }
}
