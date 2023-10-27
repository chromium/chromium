// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_actions.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

///////////////////////////////////////////////////////////////////////////////
// PinnedToolbarActionsContainer::PinnedActionToolbarButton:

// TODO(b/299463180): Add right click context menus with an option for pinning
// unpinning.
PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    PinnedActionToolbarButton(Browser* browser, actions::ActionId action_id)
    : ToolbarButton(
          base::BindRepeating(&PinnedActionToolbarButton::ButtonPressed,
                              base::Unretained(this)),
          nullptr,
          nullptr),
      action_item_(actions::ActionManager::Get().FindAction(
          action_id,
          BrowserActions::FromBrowser(browser)->root_action_item())) {
  CHECK(action_item_);
  action_changed_subscription_ = action_item_->AddActionChangedCallback(
      base::BindRepeating(&PinnedToolbarActionsContainer::
                              PinnedActionToolbarButton::ActionItemChanged,
                          base::Unretained(this)));
  ActionItemChanged();
  OnPropertyChanged(&action_item_, static_cast<views::PropertyEffects>(
                                       views::kPropertyEffectsLayout |
                                       views::kPropertyEffectsPaint));

  GetViewAccessibility().OverrideDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);

  // Normally, the notify action is determined by whether a view is draggable
  // (and is set to press for non-draggable and release for draggable views).
  // However, PinnedActionToolbarButton may be draggable or non-draggable
  // depending on whether they are shown in an incognito window or unpinned and
  // popped-out. We want to preserve the same trigger event to keep the UX
  // (more) consistent. Set all PinnedActionToolbarButton to trigger on mouse
  // release.
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnRelease);

  // Do not flip the icon for RTL languages.
  SetFlipCanvasOnPaintForRTLUI(false);
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    ~PinnedActionToolbarButton() = default;

actions::ActionId
PinnedToolbarActionsContainer::PinnedActionToolbarButton::GetActionId() {
  return *action_item_->GetActionId();
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::ButtonPressed() {
  action_item_->InvokeAction();
}

bool PinnedToolbarActionsContainer::PinnedActionToolbarButton::IsActive() {
  return anchor_higlight_.has_value();
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::AddHighlight() {
  anchor_higlight_ = AddAnchorHighlight();
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    ResetHighlight() {
  anchor_higlight_.reset();
}

void PinnedToolbarActionsContainer::PinnedActionToolbarButton::
    ActionItemChanged() {
  auto tooltip_text = action_item_->GetTooltipText().empty()
                          ? action_item_->GetText()
                          : action_item_->GetTooltipText();
  SetTooltipText(tooltip_text);
  if (!action_item_->GetAccessibleName().empty()) {
    SetAccessibleName(action_item_->GetAccessibleName());
  }
  SetImageModel(views::Button::STATE_NORMAL, action_item_->GetImage());
  SetEnabled(action_item_->GetEnabled());
  SetVisible(action_item_->GetVisible());
}

///////////////////////////////////////////////////////////////////////////////
// PinnedToolbarActionsContainer:

PinnedToolbarActionsContainer::PinnedToolbarActionsContainer(
    BrowserView* browser_view)
    : ToolbarIconContainerView(/*uses_highlight=*/false),
      browser_view_(browser_view),
      model_(PinnedToolbarActionsModel::Get(browser_view->GetProfile())) {
  // So we only get enter/exit messages when the mouse enters/exits the whole
  // container, even if it is entering/exiting a specific toolbar pinned
  // button view, too.
  SetNotifyEnterExitOnChild(true);

  model_observation_.Observe(model_.get());

  const views::FlexSpecification hide_icon_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithWeight(0);
  GetTargetLayoutManager()
      ->SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse)
      .SetDefault(views::kFlexBehaviorKey,
                  hide_icon_flex_specification.WithOrder(3));

  CreatePinnedActionButtons();
}

PinnedToolbarActionsContainer::~PinnedToolbarActionsContainer() = default;

void PinnedToolbarActionsContainer::UpdateActionState(actions::ActionId id,
                                                      bool is_active) {
  auto* button = GetPinnedButtonFor(id);
  bool pinned = button != nullptr;

  // Get or create popped out button if not pinned.
  if (!pinned) {
    button = GetPoppedOutButtonFor(id);
    if (!button && is_active) {
      button = AddPopOutButtonFor(id);
    }
  }

  // Update button highlight and force visibility if the button is active.
  if (is_active) {
    button->AddHighlight();
    button->SetProperty(views::kFlexBehaviorKey, views::FlexSpecification());
  } else {
    button->ResetHighlight();
    button->ClearProperty(views::kFlexBehaviorKey);
  }

  if (!pinned && !is_active) {
    RemovePoppedOutButtonFor(id);
  }
}

void PinnedToolbarActionsContainer::UpdateAllIcons() {
  for (auto* const pinned_button : pinned_buttons_) {
    pinned_button->UpdateIcon();
  }
}

void PinnedToolbarActionsContainer::OnActionAdded(const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      pinned_buttons_, id, [](auto* button) { return button->GetActionId(); });
  if (iter != pinned_buttons_.end()) {
    return;
  }
  AddPinnedActionButtonFor(id);
  GetSidePanelCoordinator()->UpdateHeaderPinButtonState();
}

void PinnedToolbarActionsContainer::OnActionRemoved(
    const actions::ActionId& id) {
  RemovePinnedActionButtonFor(id);
  GetSidePanelCoordinator()->UpdateHeaderPinButtonState();
}

void PinnedToolbarActionsContainer::OnActionMoved(const actions::ActionId& id,
                                                  int from_index,
                                                  int to_index) {
  ReorderChildView(GetPinnedButtonFor(id), to_index);
}

void PinnedToolbarActionsContainer::CreatePinnedActionButtons() {
  DCHECK(pinned_buttons_.empty());
  for (const auto& id : model_->pinned_action_ids()) {
    AddPinnedActionButtonFor(id);
  }
}

actions::ActionItem* PinnedToolbarActionsContainer::GetActionItemFor(
    const actions::ActionId& id) {
  return actions::ActionManager::Get().FindAction(
      id, BrowserActions::FromBrowser(browser_view_->browser())
              ->root_action_item());
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton*
PinnedToolbarActionsContainer::AddPopOutButtonFor(const actions::ActionId& id) {
  CHECK(GetActionItemFor(id));
  auto popped_out_button =
      std::make_unique<PinnedActionToolbarButton>(browser_view_->browser(), id);
  auto* button = popped_out_button.get();
  popped_out_buttons_.push_back(AddChildView(std::move(popped_out_button)));
  return button;
}

void PinnedToolbarActionsContainer::RemovePoppedOutButtonFor(
    const actions::ActionId& id) {
  const auto iter =
      base::ranges::find(popped_out_buttons_, id,
                         [](auto* button) { return button->GetActionId(); });
  if (iter == popped_out_buttons_.end()) {
    return;
  }
  // This returns a unique_ptr which is immediately destroyed.
  RemoveChildViewT(*iter);
  popped_out_buttons_.erase(iter);
}

void PinnedToolbarActionsContainer::AddPinnedActionButtonFor(
    const actions::ActionId& id) {
  actions::ActionItem* action_item = GetActionItemFor(id);
  // If the action item doesn't exist (i.e. a new id synced from an
  // update-to-date device to an out-of-date device) we do not want to create a
  // toolbar button for it.
  if (!action_item) {
    return;
  }
  if (GetPoppedOutButtonFor(id)) {
    const auto iter =
        base::ranges::find(popped_out_buttons_, id,
                           [](auto* button) { return button->GetActionId(); });
    pinned_buttons_.push_back(*iter);
    popped_out_buttons_.erase(iter);
  } else {
    auto button = std::make_unique<PinnedActionToolbarButton>(
        browser_view_->browser(), id);
    pinned_buttons_.push_back(AddChildView(std::move(button)));
  }
  ReorderViews();
}

void PinnedToolbarActionsContainer::RemovePinnedActionButtonFor(
    const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      pinned_buttons_, id, [](auto* button) { return button->GetActionId(); });
  if (iter == pinned_buttons_.end()) {
    return;
  }
  if (!(*iter)->IsActive()) {
    RemoveChildViewT(*iter);
  } else {
    popped_out_buttons_.push_back(*iter);
    ReorderViews();
  }
  pinned_buttons_.erase(iter);
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton*
PinnedToolbarActionsContainer::GetPinnedButtonFor(const actions::ActionId& id) {
  const auto iter = base::ranges::find(
      pinned_buttons_, id, [](auto* button) { return button->GetActionId(); });
  return iter == pinned_buttons_.end() ? nullptr : *iter;
}

PinnedToolbarActionsContainer::PinnedActionToolbarButton*
PinnedToolbarActionsContainer::GetPoppedOutButtonFor(
    const actions::ActionId& id) {
  const auto iter =
      base::ranges::find(popped_out_buttons_, id,
                         [](auto* button) { return button->GetActionId(); });
  return iter == popped_out_buttons_.end() ? nullptr : *iter;
}

void PinnedToolbarActionsContainer::ReorderViews() {
  size_t index = 0;
  for (auto* pinned_button : pinned_buttons_) {
    ReorderChildView(pinned_button, index);
    index++;
  }
  for (auto* popped_out_button : popped_out_buttons_) {
    ReorderChildView(popped_out_button, index);
    index++;
  }
}

SidePanelCoordinator* PinnedToolbarActionsContainer::GetSidePanelCoordinator() {
  return SidePanelUtil::GetSidePanelCoordinatorForBrowser(
      browser_view_->browser());
}
