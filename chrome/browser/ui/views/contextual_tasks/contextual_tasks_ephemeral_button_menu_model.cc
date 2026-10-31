// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_menu_model.h"

#include <optional>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/grit/branded_strings.h"
#include "components/contextual_tasks/public/features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"

namespace {

enum ContextualTasksButtonContextMenuCommand {
  kCommandRemoveForCurrentTask = 1,
  kCommandPinToToolbar = 2,
  kCommandUnpinFromToolbar = 3,
  kCommandCustomizeToolbar = 4,
};

std::optional<size_t> GetPinnedActionMenuIndexForCommand(
    const PinnedActionToolbarButtonMenuModel* menu_model,
    int command_id) {
  constexpr size_t kPinnedActionMenuDefaultItemCount = 3;
  constexpr size_t kPinnedActionMenuPinOffset = 0;
  constexpr size_t kPinnedActionMenuUnpinOffset = 1;
  constexpr size_t kPinnedActionMenuCustomizeOffset = 2;
  if (!menu_model ||
      menu_model->GetItemCount() < kPinnedActionMenuDefaultItemCount) {
    return std::nullopt;
  }
  const size_t base_index =
      menu_model->GetItemCount() - kPinnedActionMenuDefaultItemCount;
  switch (command_id) {
    case kCommandPinToToolbar:
      return base_index + kPinnedActionMenuPinOffset;
    case kCommandUnpinFromToolbar:
      return base_index + kPinnedActionMenuUnpinOffset;
    case kCommandCustomizeToolbar:
      return base_index + kPinnedActionMenuCustomizeOffset;
    default:
      return std::nullopt;
  }
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksEphemeralButtonMenuModel,
                                      kRemoveForTaskMenuItem);

ContextualTasksEphemeralButtonMenuModel::
    ContextualTasksEphemeralButtonMenuModel(
        BrowserWindowInterface* browser_window_interface)
    : ui::SimpleMenuModel(this),
      browser_window_interface_(browser_window_interface) {
  UpdateItems();
}

ContextualTasksEphemeralButtonMenuModel::
    ~ContextualTasksEphemeralButtonMenuModel() = default;

void ContextualTasksEphemeralButtonMenuModel::UpdateItems() {
  const bool should_show_close_side_panel = ShouldShowCloseSidePanel();
  const bool is_pinning_eligible = IsPinningEligible();

  Clear();
  if (should_show_close_side_panel) {
    const int close_label_id =
        contextual_tasks::ShouldUseCloseSidePanelForEphemeralButtonContextMenu()
            ? IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTEXT_MENU_CLOSE
            : IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTEXT_MENU_REMOVE;
    AddItemWithStringIdAndIcon(
        kCommandRemoveForCurrentTask, close_label_id,
        ui::ImageModel::FromVectorIcon(
            features::IsRoundedIconsEnabled() ? vector_icons::kCloseIcon
                                              : vector_icons::kCloseOldIcon,
            ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize));
    SetElementIdentifierAt(0, kRemoveForTaskMenuItem);
  }

  if (is_pinning_eligible) {
    if (should_show_close_side_panel) {
      AddSeparator(ui::NORMAL_SEPARATOR);
    }

    if (!pinned_action_menu_model_) {
      pinned_action_menu_model_ =
          std::make_unique<PinnedActionToolbarButtonMenuModel>(
              browser_window_interface_, kActionSidePanelShowContextualTasks);
    }
    constexpr int kPinnedMenuCommands[] = {
        kCommandPinToToolbar,
        kCommandUnpinFromToolbar,
        kCommandCustomizeToolbar,
    };
    for (const int command_id : kPinnedMenuCommands) {
      const std::optional<size_t> index = GetPinnedActionMenuIndexForCommand(
          pinned_action_menu_model_.get(), command_id);
      if (!index.has_value()) {
        continue;
      }
      AddItemWithIcon(command_id, pinned_action_menu_model_->GetLabelAt(*index),
                      pinned_action_menu_model_->GetIconAt(*index));
      SetElementIdentifierAt(
          GetItemCount() - 1,
          pinned_action_menu_model_->GetElementIdentifierAt(*index));
    }
  } else {
    pinned_action_menu_model_.reset();
  }
}

bool ContextualTasksEphemeralButtonMenuModel::IsCommandIdEnabled(
    int command_id) const {
  if (command_id == kCommandRemoveForCurrentTask) {
    return ShouldShowCloseSidePanel();
  }
  if (std::optional<size_t> index = GetPinnedActionMenuIndexForCommand(
          pinned_action_menu_model_.get(), command_id);
      index.has_value()) {
    return IsPinningEligible() && pinned_action_menu_model_ &&
           pinned_action_menu_model_->IsEnabledAt(*index);
  }
  return true;
}

bool ContextualTasksEphemeralButtonMenuModel::IsCommandIdVisible(
    int command_id) const {
  if (command_id == kCommandRemoveForCurrentTask) {
    return ShouldShowCloseSidePanel();
  }
  if (std::optional<size_t> index = GetPinnedActionMenuIndexForCommand(
          pinned_action_menu_model_.get(), command_id);
      index.has_value()) {
    return IsPinningEligible() && pinned_action_menu_model_ &&
           pinned_action_menu_model_->IsVisibleAt(*index);
  }
  return true;
}

void ContextualTasksEphemeralButtonMenuModel::ExecuteCommand(int command_id,
                                                             int event_flags) {
  if (command_id == kCommandRemoveForCurrentTask) {
    if (auto* const controller = ContextualTasksEphemeralButtonController::From(
            browser_window_interface_)) {
      controller->RemoveEphemeralButtonForCurrentTask();
    }
    return;
  }
  if (std::optional<size_t> index = GetPinnedActionMenuIndexForCommand(
          pinned_action_menu_model_.get(), command_id);
      index.has_value() && IsPinningEligible() && pinned_action_menu_model_) {
    if (command_id == kCommandPinToToolbar) {
      base::RecordAction(base::UserMetricsAction(
          "ContextualTasks.EphemeralToolbarButton.UserAction."
          "PinByContextMenu"));
      base::UmaHistogramBoolean(
          "ContextualTasks.EphemeralToolbarButton.UserAction."
          "PinByContextMenu",
          true);
    }
    pinned_action_menu_model_->ActivatedAt(*index, event_flags);
  }
}

void ContextualTasksEphemeralButtonMenuModel::OnMenuWillShow(
    ui::SimpleMenuModel* source) {
  base::RecordAction(base::UserMetricsAction(
      "ContextualTasks.EphemeralToolbarButton.UserAction.OpenContextMenu"));
  base::UmaHistogramBoolean(
      "ContextualTasks.EphemeralToolbarButton.UserAction.OpenContextMenu",
      true);

  if (ShouldShowCloseSidePanel()) {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.EphemeralToolbarButton.UserAction."
        "OpenContextMenu.CloseSidePanelOptionVisible"));
    base::UmaHistogramBoolean(
        "ContextualTasks.EphemeralToolbarButton.UserAction."
        "OpenContextMenu.CloseSidePanelOptionVisible",
        true);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.EphemeralToolbarButton.UserAction."
        "OpenContextMenu.CloseSidePanelOptionHidden"));
    base::UmaHistogramBoolean(
        "ContextualTasks.EphemeralToolbarButton.UserAction."
        "OpenContextMenu.CloseSidePanelOptionHidden",
        true);
  }
}

bool ContextualTasksEphemeralButtonMenuModel::ShouldShowCloseSidePanel() const {
  if (!browser_window_interface_ || !browser_window_interface_->GetProfile()) {
    return false;
  }
  if (contextual_tasks::GetEffectivePinState(
          browser_window_interface_->GetProfile())) {
    return false;
  }
  const auto* const ephemeral_controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  const auto* const panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(
          browser_window_interface_);
  const bool is_panel_open =
      ephemeral_controller ? (ephemeral_controller->IsPanelOpen() &&
                              panel_controller &&
                              panel_controller->IsPanelOpenForContextualTask())
                           : (panel_controller &&
                              panel_controller->IsPanelOpenForContextualTask());
  return !is_panel_open;
}

bool ContextualTasksEphemeralButtonMenuModel::IsPinningEligible() const {
  return browser_window_interface_ &&
         contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
             browser_window_interface_->GetProfile());
}
