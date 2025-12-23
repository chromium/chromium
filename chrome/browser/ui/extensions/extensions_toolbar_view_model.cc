// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

ExtensionsToolbarViewModel::ExtensionsToolbarViewModel(
    Delegate* delegate,
    ToolbarActionsModel* actions_model)
    : delegate_(delegate), actions_model_(actions_model) {
  actions_model_observation_.Observe(actions_model_);

  if (actions_model_->actions_initialized()) {
    OnToolbarModelInitialized();
  }
}

ExtensionsToolbarViewModel::~ExtensionsToolbarViewModel() = default;

void ExtensionsToolbarViewModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionsToolbarViewModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ToolbarActionViewModel* ExtensionsToolbarViewModel::GetActionModelForId(
    const ToolbarActionsModel::ActionId& action_id) {
  auto it = actions_.find(action_id);
  if (it == actions_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void ExtensionsToolbarViewModel::MovePinnedAction(
    const ToolbarActionsModel::ActionId& action_id,
    size_t target_index) {
  actions_model_->MovePinnedAction(action_id, target_index);
}

void ExtensionsToolbarViewModel::MovePinnedActionBy(
    const std::string& action_id,
    int move_by) {
  // Find the action's current index and verify that it's currently pinned.
  auto iter = std::ranges::find(actions_model_->pinned_action_ids(), action_id);
  CHECK(iter != actions_model_->pinned_action_ids().cend());

  // Calculate the target index, clamping it between 0 and `size - 1` to prevent
  // out-of-bounds errors.
  int current_index = iter - actions_model_->pinned_action_ids().cbegin();
  int new_index = std::clamp(
      current_index + move_by, 0,
      static_cast<int>(actions_model_->pinned_action_ids().size()) - 1);
  if (new_index == current_index) {
    return;
  }
  MovePinnedAction(action_id, new_index);
}

const base::flat_set<ToolbarActionsModel::ActionId>&
ExtensionsToolbarViewModel::GetAllActionIds() const {
  return actions_model_->action_ids();
}

const std::vector<ToolbarActionsModel::ActionId>&
ExtensionsToolbarViewModel::GetPinnedActionIds() const {
  return actions_model_->pinned_action_ids();
}

bool ExtensionsToolbarViewModel::AreActionsInitialized() {
  return actions_model_->actions_initialized();
}

bool ExtensionsToolbarViewModel::AnyActionHasCurrentSiteAccess(
    content::WebContents* web_contents) {
  for (const auto& [action_id, model] : actions_) {
    if (model->GetSiteInteraction(web_contents) ==
        extensions::SitePermissionsHelper::SiteInteraction::kGranted) {
      return true;
    }
  }
  return false;
}

void ExtensionsToolbarViewModel::OnToolbarModelInitialized() {
  CHECK(actions_.empty());
  CHECK(actions_model_->actions_initialized());

  for (const auto& action_id : actions_model_->action_ids()) {
    AppendActionModel(action_id);
  }

  for (Observer& obs : observers_) {
    obs.OnActionsInitialized();
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  AppendActionModel(action_id);

  for (Observer& obs : observers_) {
    obs.OnActionAdded(action_id);
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  auto iter = actions_.find(action_id);
  CHECK(iter != actions_.end());

  // Transfer ownership to a local variable to ensure the model remains alive
  // during the subsequent UI cleanup notifications.
  std::unique_ptr<ToolbarActionViewModel> model = std::move(iter->second);

  actions_.erase(iter);

  for (Observer& obs : observers_) {
    obs.OnActionRemoved(action_id);
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  for (Observer& obs : observers_) {
    obs.OnActionUpdated(action_id);
  }
}

void ExtensionsToolbarViewModel::OnToolbarPinnedActionsChanged() {
  for (Observer& obs : observers_) {
    obs.OnPinnedActionsChanged();
  }
}

void ExtensionsToolbarViewModel::AppendActionModel(
    const ToolbarActionsModel::ActionId& action_id) {
  actions_.emplace(action_id, delegate_->CreateActionViewModel(action_id));
}
