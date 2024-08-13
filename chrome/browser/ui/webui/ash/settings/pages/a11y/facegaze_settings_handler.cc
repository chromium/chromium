// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/facegaze_settings_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "content/public/browser/web_ui.h"

namespace ash::settings {

FaceGazeSettingsHandler::FaceGazeSettingsHandler() = default;

FaceGazeSettingsHandler::~FaceGazeSettingsHandler() {}

void FaceGazeSettingsHandler::HandleToggleGestureInfoForSettings(
    const base::Value::List& args) {
  DCHECK_EQ(args.size(), 1U);
  const bool& enabled = args[0].GetBool();

  AccessibilityManager::Get()->ToggleGestureInfoForSettings(enabled);
}

void FaceGazeSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "toggleGestureInfoForSettings",
      base::BindRepeating(
          &FaceGazeSettingsHandler::HandleToggleGestureInfoForSettings,
          base::Unretained(this)));
}

}  // namespace ash::settings
