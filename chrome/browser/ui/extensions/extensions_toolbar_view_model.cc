// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

ExtensionsToolbarViewModel::ExtensionsToolbarViewModel(
    ToolbarActionsModel* actions_model)
    : actions_model_(actions_model) {
  actions_model_observation_.Observe(actions_model_);
}

ExtensionsToolbarViewModel::~ExtensionsToolbarViewModel() = default;

void ExtensionsToolbarViewModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionsToolbarViewModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionsToolbarViewModel::AddAction(
    const ToolbarActionsModel::ActionId& action_id,
    BrowserWindowInterface* browser,
    std::unique_ptr<ExtensionActionPlatformDelegate> platform_delegate) {
  actions_.push_back(ExtensionActionViewModel::Create(
      action_id, browser, std::move(platform_delegate)));
}

std::unique_ptr<ToolbarActionViewModel>
ExtensionsToolbarViewModel::RemoveAction(
    const ToolbarActionsModel::ActionId& action_id) {
  auto iter =
      std::ranges::find(actions_, action_id, &ToolbarActionViewModel::GetId);
  CHECK(iter != actions_.end());
  std::unique_ptr<ToolbarActionViewModel> model = std::move(*iter);
  actions_.erase(iter);
  return model;
}

void ExtensionsToolbarViewModel::OnToolbarModelInitialized() {
  for (Observer& obs : observers_) {
    obs.OnActionsInitialized();
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  for (Observer& obs : observers_) {
    obs.OnActionAdded(action_id);
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
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
