// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_controller.h"

#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"

PinnedToolbarActionsController::PinnedToolbarActionsController(
    PinnedToolbarActionsContainer* container)
    : container_(container) {
  CHECK(container_);
}

PinnedToolbarActionsController::~PinnedToolbarActionsController() = default;
void PinnedToolbarActionsController::TearDown() {
  container_ = nullptr;
}

void PinnedToolbarActionsController::ShowActionEphemerallyInToolbar(
    actions::ActionId id,
    bool is_active) {
  container_->ShowActionEphemerallyInToolbar(id, is_active);
}

bool PinnedToolbarActionsController::IsActionPoppedOut(actions::ActionId id) {
  return container_->IsActionPoppedOut(id);
}

PinnedActionToolbarButton* PinnedToolbarActionsController::GetButtonFor(
    actions::ActionId id) {
  return container_->GetButtonFor(id);
}
