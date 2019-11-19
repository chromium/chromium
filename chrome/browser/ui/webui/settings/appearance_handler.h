// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

class Profile;

namespace settings {

// Chrome "Appearance" settings page UI handler.
class AppearanceHandler : public SettingsPageUIHandler {
 public:
  explicit AppearanceHandler(content::WebUI* webui);
  ~AppearanceHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Changes the UI theme of the browser to the default theme.
  void HandleUseDefaultTheme(const base::ListValue* args);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Changes the UI theme of the browser to the system (GTK+) theme.
  void HandleUseSystemTheme(const base::ListValue* args);
#endif

  Profile* profile_;  // Weak pointer.

  base::WeakPtrFactory<AppearanceHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppearanceHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
