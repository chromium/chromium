// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_WALLPAPER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_WALLPAPER_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

class Profile;

namespace chromeos {
namespace settings {

// Chrome "Personalization" settings page UI handler.
class WallpaperHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit WallpaperHandler(content::WebUI* webui);
  ~WallpaperHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Whether the wallpaper setting should be shown.
  void HandleIsWallpaperSettingVisible(const base::ListValue* args);

  // Whether the wallpaper is policy controlled.
  void HandleIsWallpaperPolicyControlled(const base::ListValue* args);

  // Open the wallpaper manager app.
  void HandleOpenWallpaperManager(const base::ListValue* args);

  // Helper function to resolve the Javascript callback.
  void ResolveCallback(const base::Value& callback_id, bool result);

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_WALLPAPER_HANDLER_H_
