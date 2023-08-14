// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/status_area_tester/status_area_tester_handler.h"

#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace ash {

// static
const char StatusAreaTesterHandler::kToggleIme[] = "toggleIme";
const char StatusAreaTesterHandler::kTogglePalette[] = "togglePalette";

StatusAreaTesterHandler::StatusAreaTesterHandler() = default;

StatusAreaTesterHandler::~StatusAreaTesterHandler() = default;

void StatusAreaTesterHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kToggleIme, base::BindRepeating(&StatusAreaTesterHandler::ToggleImeTray,
                                      weak_pointer_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kTogglePalette,
      base::BindRepeating(&StatusAreaTesterHandler::TogglePaletteTray,
                          weak_pointer_factory_.GetWeakPtr()));
}

void StatusAreaTesterHandler::SetWebUiForTesting(content::WebUI* web_ui) {
  DCHECK(web_ui);
  set_web_ui(web_ui);
}

void StatusAreaTesterHandler::ToggleImeTray(const base::Value::List& args) {
  AllowJavascript();

  // Parse JS args.
  bool toggled = args[0].GetBool();
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(/*show=*/toggled);
}

void StatusAreaTesterHandler::TogglePaletteTray(const base::Value::List& args) {
  AllowJavascript();

  // Parse JS args.
  bool toggled = args[0].GetBool();

  if (toggled) {
    stylus_utils::SetHasStylusInputForTesting();
  } else {
    stylus_utils::SetNoStylusInputForTesting();
  }

  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    root_window_controller->GetStatusAreaWidget()
        ->palette_tray()
        ->SetDisplayHasStylusForTesting();
  }
}

}  // namespace ash
