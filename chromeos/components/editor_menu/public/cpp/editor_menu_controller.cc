// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/editor_menu/public/cpp/editor_menu_controller.h"

#include "base/check_op.h"

namespace chromeos::editor_menu {

namespace {

EditorMenuController* g_instance = nullptr;

}  // namespace

EditorMenuController::EditorMenuController() {
  DCHECK(!g_instance);
  g_instance = this;
}

EditorMenuController::~EditorMenuController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

EditorMenuController* EditorMenuController::Get() {
  return g_instance;
}

}  // namespace chromeos::editor_menu
