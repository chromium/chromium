// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PARENTAL_CONTROLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PARENTAL_CONTROLS_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace chromeos {
namespace settings {

// Chrome "Parental Controls" settings page UI handler.
class ParentalControlsHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit ParentalControlsHandler(Profile* profile);
  ~ParentalControlsHandler() override;

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // ::settings::SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callbacks for handling chrome.send() events.
  void HandleShowAddSupervisionDialog(const base::ListValue* args);
  void HandleLaunchFamilyLinkSettings(const base::ListValue* args);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ParentalControlsHandler);
};

// Indicates whether parental controls should be shown in the settings UI.
bool ShouldShowParentalControls(Profile* profile);

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PARENTAL_CONTROLS_HANDLER_H_
