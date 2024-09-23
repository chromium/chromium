// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace content {
class WebUI;
}

class Profile;

namespace settings {

// Chrome "Appearance" settings page UI handler.
class AppearanceHandler : public SettingsPageUIHandler {
 public:
  explicit AppearanceHandler(content::WebUI* webui);

  AppearanceHandler(const AppearanceHandler&) = delete;
  AppearanceHandler& operator=(const AppearanceHandler&) = delete;

  ~AppearanceHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Changes the UI theme to the specified `system_theme`.
  void HandleUseTheme(ui::SystemTheme system_theme,
                      const base::Value::List& args);
  // Opens the Customize Chrome side panel.
  void OpenCustomizeChrome(const base::Value::List& args);
  // Opens the Customize Chrome side panel to the toolbar section.
  void OpenCustomizeChromeToolbarSection(const base::Value::List& args);
  // Reset toolbar pinning to the default settings.
  void ResetPinnedToolbarActions(const base::Value::List& args);
  // Whether toolbar pinning is in its default state or not.
  void PinnedToolbarActionsAreDefault(const base::Value::List& args);

  raw_ptr<Profile> profile_;  // Weak pointer.

  base::WeakPtrFactory<AppearanceHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
