// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/facegaze_settings_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "content/public/browser/web_ui.h"

namespace ash::settings {

FaceGazeSettingsHandler::FaceGazeSettingsHandler() {
  AccessibilityManager::Get()->AddFaceGazeSettingsEventHandler(this);
}

FaceGazeSettingsHandler::~FaceGazeSettingsHandler() {
  AccessibilityManager::Get()->RemoveFaceGazeSettingsEventHandler();
}

void FaceGazeSettingsHandler::HandleToggleGestureInfoForSettings(
    const base::Value::List& args) {
  DCHECK_EQ(args.size(), 1U);
  const bool& enabled = args[0].GetBool();

  AccessibilityManager::Get()->ToggleGestureInfoForSettings(enabled);
}

void FaceGazeSettingsHandler::HandleSendGestureInfoToSettings(
    const std::vector<ash::FaceGazeGestureInfo>& gesture_info) {
  base::Value::List gesture_info_list;
  for (const auto& gesture_info_entry : gesture_info) {
    base::Value::Dict gesture;
    gesture.Set("gesture", gesture_info_entry.gesture);
    gesture.Set("confidence", gesture_info_entry.confidence);
    gesture_info_list.Append(std::move(gesture));
  }

  AllowJavascript();
  FireWebUIListener("settings.sendGestureInfoToSettings", gesture_info_list);
}

void FaceGazeSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "toggleGestureInfoForSettings",
      base::BindRepeating(
          &FaceGazeSettingsHandler::HandleToggleGestureInfoForSettings,
          base::Unretained(this)));
}

}  // namespace ash::settings
