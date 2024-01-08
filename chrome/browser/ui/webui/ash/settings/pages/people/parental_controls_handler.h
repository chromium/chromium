// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_PARENTAL_CONTROLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_PARENTAL_CONTROLS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace ash::settings {

// Chrome "Parental Controls" settings page UI handler.
class ParentalControlsHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit ParentalControlsHandler(Profile* profile);

  ParentalControlsHandler(const ParentalControlsHandler&) = delete;
  ParentalControlsHandler& operator=(const ParentalControlsHandler&) = delete;

  ~ParentalControlsHandler() override;

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // ::settings::SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callbacks for handling chrome.send() events.
  void HandleShowAddSupervisionDialog(const base::Value::List& args);
  void HandleLaunchFamilyLinkSettings(const base::Value::List& args);

  raw_ptr<Profile> profile_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_PARENTAL_CONTROLS_HANDLER_H_
