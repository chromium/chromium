// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"

#include "base/functional/callback_helpers.h"

namespace tabs {

GlicActorTaskIconController::GlicActorTaskIconController(
    BrowserWindowInterface* browser_window_interface) {}

GlicActorTaskIconController::~GlicActorTaskIconController() = default;

void GlicActorTaskIconController::OnStateUpdate() {
  // TODO(crbug.com/422439520): Implement state updates for the task icon
}

}  // namespace tabs
