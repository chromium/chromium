// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PERSONALIZATION_PERSONALIZATION_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PERSONALIZATION_PERSONALIZATION_HUB_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

#include "base/values.h"

namespace ash::settings {

// Chrome "Personalization Hub" settings page UI handler.
class PersonalizationHubHandler : public ::settings::SettingsPageUIHandler {
 public:
  PersonalizationHubHandler();

  PersonalizationHubHandler(const PersonalizationHubHandler&) = delete;
  PersonalizationHubHandler& operator=(const PersonalizationHubHandler&) =
      delete;

  ~PersonalizationHubHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleOpenPersonalizationHub(const base::Value::List& args);
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PERSONALIZATION_PERSONALIZATION_HUB_HANDLER_H_
