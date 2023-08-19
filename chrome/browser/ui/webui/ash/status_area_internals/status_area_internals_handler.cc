// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/status_area_internals/status_area_internals_handler.h"

#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace ash {

// static
const char StatusAreaInternalsHandler::kToggleIme[] = "toggleIme";
const char StatusAreaInternalsHandler::kTogglePalette[] = "togglePalette";
const char StatusAreaInternalsHandler::kTriggerPrivacyIndicators[] =
    "triggerPrivacyIndicators";

StatusAreaInternalsHandler::StatusAreaInternalsHandler() = default;

StatusAreaInternalsHandler::~StatusAreaInternalsHandler() = default;

void StatusAreaInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kToggleIme,
      base::BindRepeating(&StatusAreaInternalsHandler::ToggleImeTray,
                          weak_pointer_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kTogglePalette,
      base::BindRepeating(&StatusAreaInternalsHandler::TogglePaletteTray,
                          weak_pointer_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kTriggerPrivacyIndicators,
      base::BindRepeating(&StatusAreaInternalsHandler::TriggerPrivacyIndicators,
                          weak_pointer_factory_.GetWeakPtr()));
}

void StatusAreaInternalsHandler::SetWebUiForTesting(content::WebUI* web_ui) {
  DCHECK(web_ui);
  set_web_ui(web_ui);
}

void StatusAreaInternalsHandler::ToggleImeTray(const base::Value::List& args) {
  AllowJavascript();

  // Parse JS args.
  bool toggled = args[0].GetBool();
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(/*show=*/toggled);
}

void StatusAreaInternalsHandler::TogglePaletteTray(
    const base::Value::List& args) {
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

void StatusAreaInternalsHandler::TriggerPrivacyIndicators(
    const base::Value::List& args) {
  AllowJavascript();

  // Parse JS args.
  auto app_id = args[0].GetString();
  auto app_name = args[1].GetString();
  auto is_camera_used = args[2].GetBool();
  auto is_microphone_used = args[3].GetBool();

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, base::UTF8ToUTF16(app_name), is_camera_used, is_microphone_used,
      base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>(),
      PrivacyIndicatorsSource::kApps);
}

}  // namespace ash
