// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

namespace page_actions {

using PassKey = base::PassKey<PageActionController>;

PageActionController::PageActionController(
    PinnedToolbarActionsModel* pinned_actions_model,
    PageActionModelFactory* page_action_model_factory)
    : page_action_model_factory_(page_action_model_factory) {
  if (pinned_actions_model) {
    pinned_actions_observation_.Observe(pinned_actions_model);
  }
}

PageActionController::~PageActionController() = default;

void PageActionController::Initialize(
    tabs::TabInterface& tab_interface,
    const std::vector<actions::ActionId>& action_ids) {
  tab_activated_callback_subscription_ =
      tab_interface.RegisterDidActivate(base::BindRepeating(
          &PageActionController::OnTabActivated, base::Unretained(this)));
  tab_deactivated_callback_subscription_ =
      tab_interface.RegisterWillDeactivate(base::BindRepeating(
          &PageActionController::OnTabWillDeactivate, base::Unretained(this)));
  for (actions::ActionId id : action_ids) {
    Register(id, tab_interface.IsActivated());
  }
  if (pinned_actions_observation_.GetSource()) {
    PinnedActionsModelChanged();
  }
}

void PageActionController::Register(actions::ActionId action_id,
                                    bool is_tab_active) {
  std::unique_ptr<PageActionModelInterface> model = CreateModel(action_id);
  model->SetTabActive(PassKey(), is_tab_active);
  page_actions_.emplace(action_id, std::move(model));
}

void PageActionController::Show(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PassKey(), true);
}

void PageActionController::Hide(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PassKey(), false);
}

void PageActionController::ShowSuggestionChip(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowSuggestionChip(PassKey(), true);
}

void PageActionController::HideSuggestionChip(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowSuggestionChip(PassKey(), false);
}

void PageActionController::ActionItemChanged(
    const actions::ActionItem* action_item) {
  auto& model = FindPageActionModel(action_item->GetActionId().value());
  model.SetActionItemProperties(PassKey(), action_item);
}

void PageActionController::OnTabActivated(tabs::TabInterface* tab) {
  SetModelsTabActive(true);
}

void PageActionController::OnTabWillDeactivate(tabs::TabInterface* tab) {
  SetModelsTabActive(false);
}

void PageActionController::SetModelsTabActive(bool is_active) {
  for (auto& [id, model] : page_actions_) {
    model->SetTabActive(PassKey(), is_active);
  }
}

void PageActionController::OverrideText(actions::ActionId action_id,
                                        const std::u16string& override_text) {
  FindPageActionModel(action_id).SetOverrideText(PassKey(), override_text);
}

void PageActionController::ClearOverrideText(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideText(
      PassKey(), /*override_text=*/std::nullopt);
}

void PageActionController::OverrideImage(actions::ActionId action_id,
                                         const ui::ImageModel& override_image) {
  FindPageActionModel(action_id).SetOverrideImage(PassKey(), override_image);
}

void PageActionController::ClearOverrideImage(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideImage(
      PassKey(), /*override_image=*/std::nullopt);
}

void PageActionController::OverrideTooltip(
    actions::ActionId action_id,
    const std::u16string& override_tooltip) {
  FindPageActionModel(action_id).SetOverrideTooltip(PassKey(),
                                                    override_tooltip);
}

void PageActionController::ClearOverrideTooltip(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideTooltip(
      PassKey(), /*override_tooltip=*/std::nullopt);
}

void PageActionController::AddObserver(
    actions::ActionId action_id,
    base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>&
        observation) {
  observation.Observe(&FindPageActionModel(action_id));
}

base::CallbackListSubscription
PageActionController::CreateActionItemSubscription(
    actions::ActionItem* action_item) {
  base::CallbackListSubscription subscription =
      action_item->AddActionChangedCallback(
          base::BindRepeating(&PageActionController::ActionItemChanged,
                              base::Unretained(this), action_item));
  ActionItemChanged(action_item);
  return subscription;
}

void PageActionController::OnActionsChanged() {
  PinnedActionsModelChanged();
}

void PageActionController::PinnedActionsModelChanged() {
  PinnedToolbarActionsModel* pinned_actions_model =
      pinned_actions_observation_.GetSource();
  CHECK(pinned_actions_model);
  for (auto& [id, model] : page_actions_) {
    const bool is_pinned = pinned_actions_model->Contains(id);
    model->SetHasPinnedIcon(PassKey(), is_pinned);
  }
}

PageActionModelInterface& PageActionController::FindPageActionModel(
    actions::ActionId action_id) const {
  auto id_to_model = page_actions_.find(action_id);
  CHECK(id_to_model != page_actions_.end());
  CHECK(id_to_model->second.get());
  return *id_to_model->second.get();
}

std::unique_ptr<PageActionModelInterface> PageActionController::CreateModel(
    actions::ActionId action_id) {
  if (page_action_model_factory_ != nullptr) {
    return page_action_model_factory_->Create(action_id);
  } else {
    return std::make_unique<PageActionModel>();
  }
}

}  // namespace page_actions
