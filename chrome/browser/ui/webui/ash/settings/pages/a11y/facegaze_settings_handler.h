// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_FACEGAZE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_FACEGAZE_SETTINGS_HANDLER_H_

#include "chrome/browser/ash/accessibility/facegaze_settings_event_handler.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace ash {
struct FaceGazeGestureInfo;
}  // namespace ash

namespace ash::settings {

// Settings handler for the FaceGaze feature.
class FaceGazeSettingsHandler : public ::settings::SettingsPageUIHandler,
                                public ash::FaceGazeSettingsEventHandler {
 public:
  FaceGazeSettingsHandler();
  FaceGazeSettingsHandler(const FaceGazeSettingsHandler&) = delete;
  FaceGazeSettingsHandler& operator=(const FaceGazeSettingsHandler&) = delete;
  ~FaceGazeSettingsHandler() override;

  void HandleToggleGestureInfoForSettings(const base::Value::List& args);

  // ash::FaceGazeSettingsEventHandler:
  void HandleSendGestureInfoToSettings(
      const std::vector<FaceGazeGestureInfo>& gesture_info) override;

  // ::settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_FACEGAZE_SETTINGS_HANDLER_H_
