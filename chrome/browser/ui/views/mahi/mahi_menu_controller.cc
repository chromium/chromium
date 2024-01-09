// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

namespace chromeos::mahi {

MahiMenuController::MahiMenuController() = default;

MahiMenuController::~MahiMenuController() = default;

void MahiMenuController::OnContextMenuShown(Profile* profile) {
  // TODO(b/315596183): Finish this function.
}

void MahiMenuController::OnTextAvailable(const gfx::Rect& anchor_bounds,
                                         const std::string& selected_text,
                                         const std::string& surrounding_text) {
  // TODO(b/315596183): Finish this function.
}

void MahiMenuController::OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) {
  // TODO(b/315596183): Finish this function.
}

void MahiMenuController::OnDismiss(bool is_other_command_executed) {
  // TODO(b/315596183): Finish this function.
}

}  // namespace chromeos::mahi
