// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/lobster_manager.h"

#include "ash/lobster/lobster_controller.h"
#include "base/check.h"

namespace chromeos::editor_menu {

LobsterManager::LobsterManager(
    std::unique_ptr<ash::LobsterController::Trigger> trigger)
    : trigger_(std::move(trigger)) {
  CHECK(trigger_);
}

LobsterManager::~LobsterManager() = default;

void LobsterManager::StartFlow(const std::string& freeform_text) {
  trigger_->Fire(freeform_text);
}

}  // namespace chromeos::editor_menu
