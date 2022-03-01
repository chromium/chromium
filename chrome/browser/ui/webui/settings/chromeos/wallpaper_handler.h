// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_WALLPAPER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_WALLPAPER_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace chromeos {
namespace settings {

// Chrome "Personalization" settings page UI handler.
class WallpaperHandler : public ::settings::SettingsPageUIHandler {
 public:
  WallpaperHandler();

  WallpaperHandler(const WallpaperHandler&) = delete;
  WallpaperHandler& operator=(const WallpaperHandler&) = delete;

  ~WallpaperHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Whether the wallpaper setting should be shown.
  void HandleIsWallpaperSettingVisible(const base::Value::List& args);

  // Whether the wallpaper is policy controlled.
  void HandleIsWallpaperPolicyControlled(const base::Value::List& args);

  // Open the wallpaper manager app.
  void HandleOpenWallpaperManager(const base::Value::List& args);

  // Helper function to resolve the Javascript callback.
  void ResolveCallback(const base::Value& callback_id, bool result);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_WALLPAPER_HANDLER_H_
