// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_pinned_action_toolbar_button.h"

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_menu_model.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"

namespace {

// Forwards everything to the button's own menu model, so the menu looks and
// behaves exactly the same, and additionally records the Contextual Tasks
// ephemeral button context menu metrics.
class EphemeralButtonMenuModel : public ui::MenuModel {
 public:
  explicit EphemeralButtonMenuModel(ui::MenuModel& menu_model)
      : menu_model_(menu_model) {}
  EphemeralButtonMenuModel(const EphemeralButtonMenuModel&) = delete;
  EphemeralButtonMenuModel& operator=(const EphemeralButtonMenuModel&) = delete;
  ~EphemeralButtonMenuModel() override = default;

  // ui::MenuModel:
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  size_t GetItemCount() const override { return menu_model_->GetItemCount(); }
  ItemType GetTypeAt(size_t index) const override {
    return menu_model_->GetTypeAt(index);
  }
  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override {
    return menu_model_->GetSeparatorTypeAt(index);
  }
  int GetCommandIdAt(size_t index) const override {
    return menu_model_->GetCommandIdAt(index);
  }
  std::u16string GetLabelAt(size_t index) const override {
    return menu_model_->GetLabelAt(index);
  }
  bool IsItemDynamicAt(size_t index) const override {
    return menu_model_->IsItemDynamicAt(index);
  }
  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override {
    return menu_model_->GetAcceleratorAt(index, accelerator);
  }
  bool IsItemCheckedAt(size_t index) const override {
    return menu_model_->IsItemCheckedAt(index);
  }
  int GetGroupIdAt(size_t index) const override {
    return menu_model_->GetGroupIdAt(index);
  }
  ui::ImageModel GetIconAt(size_t index) const override {
    return menu_model_->GetIconAt(index);
  }
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override {
    return menu_model_->GetButtonMenuItemAt(index);
  }
  bool IsEnabledAt(size_t index) const override {
    return menu_model_->IsEnabledAt(index);
  }
  bool IsVisibleAt(size_t index) const override {
    return menu_model_->IsVisibleAt(index);
  }
  ui::MenuModel* GetSubmenuModelAt(size_t index) const override {
    return menu_model_->GetSubmenuModelAt(index);
  }
  void ActivatedAt(size_t index) override {
    MaybeRecordPinByContextMenu(index);
    menu_model_->ActivatedAt(index);
  }
  void ActivatedAt(size_t index, int event_flags) override {
    MaybeRecordPinByContextMenu(index);
    menu_model_->ActivatedAt(index, event_flags);
  }
  ui::ElementIdentifier GetElementIdentifierAt(size_t index) const override {
    return menu_model_->GetElementIdentifierAt(index);
  }
  void MenuWillShow() override {
    contextual_tasks::RecordEphemeralButtonContextMenuShown(
        IsCloseSidePanelItemVisible());
    menu_model_->MenuWillShow();
  }
  void MenuWillClose() override { menu_model_->MenuWillClose(); }

 private:
  void MaybeRecordPinByContextMenu(size_t index) {
    if (GetElementIdentifierAt(index) == kPinnedActionToolbarPinElementId) {
      contextual_tasks::RecordEphemeralButtonPinByContextMenu();
    }
  }

  // Returns whether the menu shows the item to remove the button for the
  // current task (or to close the side panel).
  bool IsCloseSidePanelItemVisible() const {
    for (size_t i = 0; i < GetItemCount(); ++i) {
      if (GetElementIdentifierAt(i) ==
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem) {
        return IsVisibleAt(i);
      }
    }
    return false;
  }

  const raw_ref<ui::MenuModel> menu_model_;
  base::WeakPtrFactory<EphemeralButtonMenuModel> weak_ptr_factory_{this};
};

// Invokes the action like `PinnedActionToolbarButtonActionViewInterface`, but
// while the button is unpinned, does not record
// `Actions.PinnedToolbarButtonActivation*`: the user never pinned the ephemeral
// button, and its presses are already recorded as
// `ContextualTasks.EphemeralToolbarButton.UserAction.*` by the action.
class EphemeralButtonActionViewInterface
    : public PinnedActionToolbarButtonActionViewInterface {
 public:
  explicit EphemeralButtonActionViewInterface(
      PinnedActionToolbarButton* action_view)
      : PinnedActionToolbarButtonActionViewInterface(action_view),
        action_view_(action_view) {}
  ~EphemeralButtonActionViewInterface() override = default;

  // PinnedActionToolbarButtonActionViewInterface:
  void InvokeActionImpl(actions::ActionItem* action_item) override {
    if (action_view_->IsPinned()) {
      PinnedActionToolbarButtonActionViewInterface::InvokeActionImpl(
          action_item);
      return;
    }
    base::AutoReset<bool> needs_delayed_destruction =
        action_view_->SetNeedsDelayedDestruction(true);
    action_item->InvokeAction(
        actions::ActionInvocationContext::Builder()
            .SetProperty(kSidePanelOpenTriggerKey,
                         SidePanelOpenTrigger::kPinnedEntryToolbarButton)
            .Build());
  }

 private:
  const raw_ptr<PinnedActionToolbarButton> action_view_;
};

}  // namespace

ContextualTasksPinnedActionToolbarButton::
    ContextualTasksPinnedActionToolbarButton(
        BrowserWindowInterface* browser,
        actions::ActionId action_id,
        base::WeakPtr<PinnedToolbarActionsContainer> container)
    : PinnedActionToolbarButton(browser, action_id, std::move(container)),
      ephemeral_menu_model_(std::make_unique<EphemeralButtonMenuModel>(
          CHECK_DEREF(menu_model()))) {}

ContextualTasksPinnedActionToolbarButton::
    ~ContextualTasksPinnedActionToolbarButton() = default;

// static
std::unique_ptr<PinnedActionToolbarButton>
ContextualTasksPinnedActionToolbarButton::Create(
    BrowserWindowInterface* browser,
    actions::ActionId action_id,
    base::WeakPtr<PinnedToolbarActionsContainer> container) {
  return std::make_unique<ContextualTasksPinnedActionToolbarButton>(
      browser, action_id, std::move(container));
}

std::unique_ptr<views::ActionViewInterface>
ContextualTasksPinnedActionToolbarButton::GetActionViewInterface() {
  return std::make_unique<EphemeralButtonActionViewInterface>(this);
}

void ContextualTasksPinnedActionToolbarButton::ShowDropDownMenu(
    ui::mojom::MenuSourceType source_type) {
  // Once pinned, this is no longer the ephemeral button, so show the regular
  // pinned toolbar button menu without the ephemeral button metrics.
  if (IsPinned() || !ShouldShowMenu()) {
    PinnedActionToolbarButton::ShowDropDownMenu(source_type);
    return;
  }
  ShowMenuForModel(source_type, ephemeral_menu_model_.get());
}

BEGIN_METADATA(ContextualTasksPinnedActionToolbarButton)
END_METADATA
