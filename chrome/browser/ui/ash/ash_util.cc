// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_util.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"

namespace ash_util {

void SetupWidgetInitParamsForContainer(views::Widget::InitParams* params,
                                       int container_id) {
  DCHECK_GE(container_id, ash::kShellWindowId_MinContainer);
  DCHECK_LE(container_id, ash::kShellWindowId_MaxContainer);

  params->parent = ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                            container_id);
}

int GetSystemModalDialogContainerId() {
  return session_manager::SessionManager::Get()->session_state() ==
                 session_manager::SessionState::ACTIVE
             ? ash::kShellWindowId_SystemModalContainer
             : ash::kShellWindowId_LockSystemModalContainer;
}

void BounceWindow(aura::Window* window) {
  wm::AnimateWindow(window, wm::WINDOW_ANIMATION_TYPE_BOUNCE);
}

}  // namespace ash_util
