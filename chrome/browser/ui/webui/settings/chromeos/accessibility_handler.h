// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_

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

class AccessibilityHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit AccessibilityHandler(content::WebUI* webui);
  ~AccessibilityHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  // Callback for the messages to show settings for ChromeVox or
  // Select To Speak.
  void HandleShowChromeVoxSettings(const base::ListValue* args);
  void HandleShowSelectToSpeakSettings(const base::ListValue* args);
  void HandleGetStartupSoundEnabled(const base::ListValue* args);
  void HandleSetStartupSoundEnabled(const base::ListValue* args);

  void OpenExtensionOptionsPage(const char extension_id[]);

  Profile* profile_;  // Weak pointer.

  DISALLOW_COPY_AND_ASSIGN(AccessibilityHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCESSIBILITY_HANDLER_H_
