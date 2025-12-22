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

bool ExtensionsToolbarViewModel::AreActionsInitialized() {
  return actions_model_->actions_initialized();
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
  auto iter =
      std::ranges::find(actions_, action_id, &ToolbarActionViewModel::GetId);
  CHECK(iter != actions_.end());
  std::unique_ptr<ToolbarActionViewModel> model = std::move(*iter);
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
  actions_.push_back(delegate_->CreateActionViewModel(action_id));
}
