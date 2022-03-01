// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
  // Changes the UI theme of the browser to the default theme.
  void HandleUseDefaultTheme(const base::Value::List& args);

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Changes the UI theme of the browser to the system (GTK+) theme.
  void HandleUseSystemTheme(const base::Value::List& args);
#endif

  raw_ptr<Profile> profile_;  // Weak pointer.

  base::WeakPtrFactory<AppearanceHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
