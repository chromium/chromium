// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_EDITOR_MENU_LOBSTER_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_EDITOR_MENU_LOBSTER_MANAGER_H_

#include "ash/lobster/lobster_controller.h"

namespace chromeos::editor_menu {

class LobsterManager {
 public:
  explicit LobsterManager(
      std::unique_ptr<ash::LobsterController::Trigger> trigger);

  ~LobsterManager();

  void StartFlow(const std::string& freeform_text);

 private:
  std::unique_ptr<ash::LobsterController::Trigger> trigger_;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_ASH_EDITOR_MENU_LOBSTER_MANAGER_H_
