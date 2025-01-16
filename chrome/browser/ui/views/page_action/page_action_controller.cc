// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

namespace page_actions {

PageActionController::PageActionController(
    const PinnedToolbarActionsModel* pinned_actions_model)
    : pinned_actions_model_(pinned_actions_model) {}

PageActionController::~PageActionController() = default;

void PageActionController::Initialize(
    const std::vector<actions::ActionId>& action_ids) {
  for (actions::ActionId id : action_ids) {
    Register(id);
  }
}

void PageActionController::Register(actions::ActionId action_id) {
  page_actions_.emplace(action_id, std::make_unique<PageActionModel>());
}

void PageActionController::Show(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(
      base::PassKey<PageActionController>(), true);
}

bool PageActionController::ShowIfNotPinned(actions::ActionId action_id) {
  if (pinned_actions_model_ && pinned_actions_model_->Contains(action_id)) {
    return false;
  }
  Show(action_id);
  return true;
}

void PageActionController::Hide(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(
      base::PassKey<PageActionController>(), false);
}

void PageActionController::ActionItemChanged(
    const actions::ActionItem* action_item) {
  auto& model = FindPageActionModel(action_item->GetActionId().value());
  model.SetActionItemProperties(base::PassKey<PageActionController>(),
                                action_item);
}

void PageActionController::OverrideText(actions::ActionId action_id,
                                        const std::u16string& override_text) {
  FindPageActionModel(action_id).SetOverrideText(
      base::PassKey<PageActionController>(), override_text);
}

void PageActionController::ClearOverrideText(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideText(
      base::PassKey<PageActionController>(), /*override_text=*/std::nullopt);
}

void PageActionController::AddObserver(
    actions::ActionId action_id,
    base::ScopedObservation<PageActionModel, PageActionModelObserver>&
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

PageActionModel& PageActionController::FindPageActionModel(
    actions::ActionId action_id) const {
  auto id_to_model = page_actions_.find(action_id);
  CHECK(id_to_model != page_actions_.end());
  CHECK(id_to_model->second.get());
  return *id_to_model->second.get();
}

}  // namespace page_actions
